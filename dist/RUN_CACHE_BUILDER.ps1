Param()

$ErrorActionPreference = 'Stop'

try {
  $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
  Set-Location $scriptDir

  $exe = Join-Path $scriptDir '_SC4PlopAndPaintCacheBuilder.exe'

  if (-not (Test-Path $exe)) {
    Write-Host "Could not find $exe"
    Write-Host "Build the CLI first or update this path in run_cli.ps1."
    Read-Host "Press Enter to exit"
    exit 1
  }

  Write-Host "=== SC4 Plop and Paint Cache Builder ==="
  Write-Host ""

  Write-Host ""
  $doScan = Read-Host "Run cache building scan now? (y/N)"
  if ($doScan -notin @('y','Y')) {
    Write-Host ""
    Write-Host "Skipping scan."
    Read-Host "Press Enter to exit"
    exit 0
  }

  Write-Host ""
  $defaultGameRoot = "$env:PROGRAMFILES(x86)\SimCity 4 Deluxe Edition"
  $defaultPlugins = "$env:USERPROFILE\Documents\SimCity 4\Plugins"
  $defaultLocale = "English"

  $gameRoot = Read-Host "Game root directory (default: $defaultGameRoot)"
  $pluginsDir = Read-Host "User Plugins directory (default: $defaultPlugins)"
  $localeDir = Read-Host "Locale under game root (default: $defaultLocale)"
  $renderThumbs = Read-Host "Render 3D thumbnails? (y/N)"

  $cmd = @($exe, '--scan')
  if ($gameRoot) { $cmd += @('--game', $gameRoot) }
  if ($pluginsDir) { $cmd += @('--plugins', $pluginsDir) }
  if ($localeDir) { $cmd += @('--locale', $localeDir) }
  if ($renderThumbs -in @('y','Y')) { $cmd += '--render-thumbnails' }

  Write-Host ""
  Write-Host "Running:"
  Write-Host ($cmd -join ' ')
  & $cmd[0] @($cmd[1..($cmd.Length-1)])

  Write-Host ""
  Read-Host "Press Enter to exit"
}
catch {
  Write-Host ""
  Write-Host "Error: $($_.Exception.Message)"
  Write-Host $_.Exception.ToString()
  Read-Host "Press Enter to exit"
  exit 1
}