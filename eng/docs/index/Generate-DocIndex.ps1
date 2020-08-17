# Generates an index page for cataloging different versions of the Docs

[CmdletBinding()]
Param (
    $RepoRoot,
    $DocGenDir
)

$ServiceMapping = @{
    "core"="Core";
    "iot"="IoT";
    "storage"="Storage";
}

Write-Verbose "Name Reccuring paths with variable names"
$DocFxTool = "${RepoRoot}/docfx/docfx.exe"
$DocOutDir = "${RepoRoot}/docfx_project"

Write-Verbose "Initializing Default DocFx Site..."
& "${DocFxTool}" init -q -o "${DocOutDir}"

Write-Verbose "Copying template and configuration..."
New-Item -Path "${DocOutDir}" -Name "templates" -ItemType "directory"
Copy-Item "${DocGenDir}/templates/*" -Destination "${DocOutDir}/templates" -Force -Recurse
Copy-Item "${DocGenDir}/docfx.json" -Destination "${DocOutDir}/" -Force

$YmlPath = "${DocOutDir}/api"
New-Item -Path $YmlPath -Name "toc.yml" -Force

Write-Verbose "Creating Index for client packages..."
foreach ($Dir in $ServiceMapping.Keys)
{
    # Generate a new top-level md file for the service
    New-Item -Path $YmlPath -Name "$($Dir).md" -Force

    # Add service to toc.yml
    $ServiceName = $ServiceMapping[$Dir]
    Add-Content -Path "$($YmlPath)/toc.yml" -Value "- name: $($ServiceName)`r`n  href: $($Dir.Name).md"

    Add-Content -Path "$($YmlPath)/$($Dir.Name).md" -Value "# Client"
    Add-Content -Path "$($YmlPath)/$($Dir.Name).md" -Value "---"
    Write-Verbose "Operating on Client Packages for $($Dir.Name)"
}

Write-Verbose "Creating Site Title and Navigation..."
New-Item -Path "${DocOutDir}" -Name "toc.yml" -Force
Add-Content -Path "${DocOutDir}/toc.yml" -Value "- name: Azure SDK for C APIs`r`n  href: api/`r`n  homepage: api/index.md"

Write-Verbose "Copying root markdowns"
Copy-Item "$($RepoRoot)/README.md" -Destination "${DocOutDir}/api/index.md" -Force
Copy-Item "$($RepoRoot)/CONTRIBUTING.md" -Destination "${DocOutDir}/api/CONTRIBUTING.md" -Force

Write-Verbose "Building site..."
& "${DocFxTool}" build "${DocOutDir}/docfx.json"

Copy-Item "${DocGenDir}/assets/logo.svg" -Destination "${DocOutDir}/" -Force
Copy-Item "${DocGenDir}/assets/toc.yml" -Destination "${DocOutDir}/" -Force
Copy-Item "${DocGenDir}/assets/logo.svg" -Destination "${DocOutDir}/_site/" -Force
Copy-Item "${DocGenDir}/assets/toc.yml" -Destination "${DocOutDir}/_site/" -Force