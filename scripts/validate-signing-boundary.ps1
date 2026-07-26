<#
.SYNOPSIS
  Validate that signing secrets stay outside Git and password handling stays out of artifact signing.
#>
[CmdletBinding()]
param([switch]$Help)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$signScript = Join-Path $PSScriptRoot 'sign-artifacts.ps1'
$managerScript = Join-Path $PSScriptRoot 'manage-signing-certificate.ps1'

$sensitivePatterns = @(
    '*.pfx',
    '*.p12',
    '*.pem',
    '*.key',
    '*.pvk',
    '*.snk',
    '*.credential.xml'
)
$trackedSensitiveFiles = @(& git -C $root ls-files -- $sensitivePatterns)
if ($LASTEXITCODE -ne 0) {
    throw 'git ls-files failed while validating signing secrets.'
}
if ($trackedSensitiveFiles.Count -gt 0) {
    $trackedSensitiveFiles | ForEach-Object {
        [pscustomobject]@{ File = $_; Category = 'tracked-signing-secret' }
    } | Format-Table -AutoSize
    throw 'Signing secret material is tracked by Git.'
}

$sensitiveSuffixes = @(
    '.pfx',
    '.p12',
    '.pem',
    '.key',
    '.pvk',
    '.snk',
    '.credential.xml'
)
$workingTreeSecrets = @(
    Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue |
    Where-Object {
        $fileName = $_.Name
        $_.FullName -notmatch '\\\.git\\' -and
        ($sensitiveSuffixes | Where-Object {
            $fileName.EndsWith($_, [System.StringComparison]::OrdinalIgnoreCase)
        })
    }
)
if ($workingTreeSecrets.Count -gt 0) {
    $workingTreeSecrets | ForEach-Object {
        [pscustomobject]@{
            File = [System.IO.Path]::GetRelativePath($root, $_.FullName)
            Category = 'working-tree-signing-secret'
        }
    } | Format-Table -AutoSize
    throw 'Signing secret material exists inside the repository working tree.'
}

$tokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $signScript,
    [ref]$tokens,
    [ref]$parseErrors)
if ($parseErrors.Count -gt 0) {
    $parseErrors | Format-Table -AutoSize
    throw 'The signing script does not parse.'
}

$forbiddenParameters = @(
    'Password',
    'Secret',
    'PfxPassword',
    'CredentialPath'
)
$parameterNames = @($ast.ParamBlock.Parameters.Name.VariablePath.UserPath)
$exposed = @($parameterNames | Where-Object { $_ -in $forbiddenParameters })
if ($exposed.Count -gt 0) {
    $exposed | ForEach-Object {
        [pscustomobject]@{ Parameter = $_; Category = 'secret-input' }
    } | Format-Table -AutoSize
    throw 'The artifact-signing script exposes a secret-bearing parameter.'
}

$forbiddenCommands = @(
    'ConvertTo-SecureString',
    'Get-Credential',
    'Import-Clixml',
    'Import-PfxCertificate',
    'Export-PfxCertificate'
)
$commandNames = @(
    $ast.FindAll({
        param($node)
        $node -is [System.Management.Automation.Language.CommandAst]
    }, $true) |
    ForEach-Object { $_.GetCommandName() } |
    Where-Object { $_ }
)
$secretCommands = @($commandNames | Where-Object { $_ -in $forbiddenCommands })
if ($secretCommands.Count -gt 0) {
    $secretCommands | Select-Object -Unique | ForEach-Object {
        [pscustomobject]@{ Command = $_; Category = 'secret-handling-in-signing-path' }
    } | Format-Table -AutoSize
    throw 'The artifact-signing script handles a secret directly.'
}

$managerText = Get-Content -Raw -LiteralPath $managerScript
$signText = Get-Content -Raw -LiteralPath $signScript
foreach ($required in @(
    [pscustomobject]@{ File = $managerScript; Text = $managerText; Marker = 'Assert-ExternalVault' },
    [pscustomobject]@{ File = $managerScript; Text = $managerText; Marker = 'KeyExportPolicy' },
    [pscustomobject]@{ File = $signScript; Text = $signText; Marker = 'Assert-ExternalVault' },
    [pscustomobject]@{ File = $signScript; Text = $signText; Marker = 'ReparsePoint' }
)) {
    if (-not $required.Text.Contains($required.Marker, [System.StringComparison]::Ordinal)) {
        throw "Required signing safety marker is missing from $($required.File): $($required.Marker)"
    }
}

Write-Output 'Signing boundary validation passed.'
