$ErrorActionPreference = 'Stop'

$project = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $project 'src'
$outputFolder = Join-Path $project 'build\unity-curl-x86'

$groups = [ordered]@{}
$allSources = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Filter '*.cpp' |
    Where-Object { $_.Name -ne 'kickchat_tls_schannel.cpp' } |
    Sort-Object FullName)

foreach ($source in $allSources) {
    $relative = $source.FullName.Substring($project.Length + 1).Replace('\', '/')
    $directory = Split-Path -Leaf $source.DirectoryName
    switch ($relative) {
        'src/app/kickchat_chat_events.cpp' { $group = 'app_events'; break }
        'src/links/kickchat_link_fetch_winhttp.cpp' { $group = 'links_fetch'; break }
        default { $group = $directory; break }
    }
    if (-not $groups.Contains($group)) { $groups[$group] = New-Object 'System.Collections.Generic.List[string]' }
    $groups[$group].Add($relative)
}

# Keep GDI+ headers ahead of other link implementation includes.
if ($groups.Contains('links')) {
    $tooltip = 'src/links/kickchat_link_tooltip.cpp'
    if ($groups['links'].Contains($tooltip)) {
        $groups['links'].Remove($tooltip) | Out-Null
        $groups['links'].Insert(0, $tooltip)
    }
}

if (-not (Test-Path -LiteralPath $outputFolder)) {
    New-Item -ItemType Directory -Path $outputFolder | Out-Null
}

$response = New-Object 'System.Collections.Generic.List[string]'
$includedSources = 0
foreach ($group in $groups.Keys) {
    $filename = 'unity_' + $group + '.cpp'
    $target = Join-Path $outputFolder $filename
    $lines = New-Object 'System.Collections.Generic.List[string]'
    $lines.Add('#include "kickchat_platform.hpp"')
    if ($group -eq 'links') { $lines.Add('#include <propidl.h>') }
    foreach ($source in $groups[$group]) {
        $lines.Add('#include "../../' + $source + '"')
        ++$includedSources
    }
    [System.IO.File]::WriteAllLines($target, $lines, (New-Object System.Text.UTF8Encoding($false)))
    $response.Add('build/unity-curl-x86/' + $filename)
    Write-Host ($group + ': ' + $groups[$group].Count + ' sources')
}

if ($includedSources -ne $allSources.Count) {
    throw ('Unity source coverage mismatch: ' + $includedSources + '/' + $allSources.Count)
}

[System.IO.File]::WriteAllLines((Join-Path $outputFolder 'unity_sources.rsp'),
    $response, (New-Object System.Text.UTF8Encoding($false)))
Write-Host ('Unity units: ' + $response.Count + '; original sources: ' + $includedSources)
