param (
  [string]$TargetDirectory = "", # Code path to code owners. e.g sdk/core/azure-amqp
  [string]$CodeOwnerFileLocation = "$PSSCriptRoot/../../../.github/CODEOWNERS", # The absolute path of CODEOWNERS file. 
  [string]$ToolVersion = "1.0.0-dev.20211122.14", # Placeholder. Will update in next PR
  [string]$ToolPath = (Join-Path ([System.IO.Path]::GetTempPath()) "codeowners-tool-path"), # The place to check the tool existence. Put temp path as default
  [string]$DevOpsFeed = "https://pkgs.dev.azure.com/azure-sdk/public/_packaging/azure-sdk-for-net/nuget/v3/index.json", # DevOp tool feeds.
  [string]$VsoVariable = "", # Option of write code owners into devop variable
  [switch]$IncludeNonUserAliases, # Option to filter out the team alias in code owner list. e.g. Azure/azure-sdk-team
  [switch]$Test  #Run test functions against the script logic
)

function Get-CodeOwnersTool()
{
  $command = Join-Path $ToolPath "retrieve-codeowners"
  # Check if the retrieve-codeowners tool exsits or not.
  if (Get-Command $command -errorAction SilentlyContinue) {
    return $command
  }
  if (!(Test-Path $ToolPath)) {
    New-Item -ItemType Directory -Path $ToolPath | Out-Null
  }
  Write-Host "Installing the retrieve-codeowners tool under $ToolPath... "
  dotnet tool install --tool-path $ToolPath --add-source $DevOpsFeed --version $ToolVersion "Azure.Sdk.Tools.RetrieveCodeOwners" | Out-Null

  # Test to see if the tool properly installed.
  if (!(Get-Command $command -errorAction SilentlyContinue)) {
    Write-Error "The retrieve-codeowners tool is not properly installed. Please check your tool path. $ToolPath"
    return 
  }
  return $command
}

function Get-CodeOwners ([string]$targetDirectory, [string]$codeOwnerFileLocation, [bool]$useNonUserAlias = $false)
{
  $command = Get-CodeOwnersTool
  # Filter out the non user alias from code owner list.
  Write-Host "Testing on $targetDirectory..."
  if($useNonUserAlias) {
    $codeOwnersString = & $command --target-directory $targetDirectory --code-owner-file-path $codeOwnerFileLocation 2>&1
  }
  else {
    $codeOwnersString = & $command --target-directory $targetDirectory --code-owner-file-path $codeOwnerFileLocation --filter-out-non-user-aliases 2>&1
  }
  # Failed at the command of fetching code owners.
  if ($LASTEXITCODE -ne 0) {
    Write-Host $codeOwnersString
    return ,@()
  }
  
  $codeOwnersJson = $codeOwnersString | ConvertFrom-Json
  if (!$codeOwnersJson) {
    Write-Host "No code owners returned from the path: $targetDirectory"
    return ,@()
  }
  
  if ($VsoVariable) {
    $codeOwners = $codeOwnersJson.Owners -join ","
    Write-Host "##vso[task.setvariable variable=$VsoVariable;]$codeOwners"
  }

  return $codeOwnersJson.Owners
}

function TestGetCodeOwner([string]$targetDirectory, [string]$codeOwnerFileLocation, [bool]$useNonUserAlias = $false, [string[]]$expectReturn) {
  $actualReturn = Get-CodeOwners -targetDirectory $targetDirectory -codeOwnerFileLocation $codeOwnerFileLocation -useNonUserAlias $useNonUserAlias

  if ($actualReturn.Count -ne $expectReturn.Count) {
    Write-Error "The length of actual result is not as expected. Expected length: $($expectReturn.Count), Actual length: $($actualReturn.Count)."
    exit 1
  }
  for ($i = 0; $i -lt $expectReturn.Count; $i++) {
    if ($expectReturn[$i] -ne $actualReturn[$i]) {
      Write-Error "Expect result $expectReturn[$i] is different than actual result $actualReturn[$i]."
      exit 1
    }
  }
}

if($Test) {
  $testFile = "$PSSCriptRoot/../../../tools/code-owners-parser/Azure.Sdk.Tools.RetrieveCodeOwners.Tests/CODEOWNERS"
  TestGetCodeOwner -targetDirectory "sdk" -codeOwnerFileLocation $testFile -useNonUserAlias $true -expectReturn @("person1", "person2")
  TestGetCodeOwner -targetDirectory "sdk/noPath" -codeOwnerFileLocation $testFile -useNonUserAlias $true -expectReturn @("person1", "person2")
  TestGetCodeOwner -targetDirectory "/sdk/azconfig" -codeOwnerFileLocation $testFile -useNonUserAlias $true -expectReturn @("person3", "person4")
  TestGetCodeOwner -targetDirectory "/sdk/azconfig/package" -codeOwnerFileLocation $testFile -useNonUserAlias $true  $testFile -expectReturn @("person3", "person4")
  TestGetCodeOwner -targetDirectory "/sd" -codeOwnerFileLocation $testFile -useNonUserAlias $true  -expectReturn @()
  TestGetCodeOwner -targetDirectory "/sdk/testUser/" -codeOwnerFileLocation $testFile -expectReturn @("azure-sdk") 
  exit 0
}
else {
  return Get-CodeOwners -targetDirectory $TargetDirectory -codeOwnerFileLocation $CodeOwnerFileLocation -useNonUserAlias $IncludeNonUserAliases
}
