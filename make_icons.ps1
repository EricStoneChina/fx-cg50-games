# =============================================================================
#  Generate official-style fx-CG50 game icons.
#
#  Every output is a 92x64, 24bpp RGB, uncompressed BMP. The unselected image
#  has a white outer canvas; the selected image adds a cyan highlight behind
#  the coloured tile while keeping the outer canvas white.
#  The coloured rounded tile stays in the top 38 pixels so the calculator can
#  use the lower area for its menu label.
#
#  This script intentionally draws the source artwork with System.Drawing so
#  the geometry is deterministic and easy to regenerate.
# =============================================================================
Add-Type -AssemblyName System.Drawing

function New-RoundRectPath([single]$x, [single]$y, [single]$w, [single]$h, [single]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-LinearBrush([single]$x1, [single]$y1, [single]$x2, [single]$y2, [System.Drawing.Color]$top, [System.Drawing.Color]$bottom) {
    $p1 = New-Object System.Drawing.PointF([single]$x1, [single]$y1)
    $p2 = New-Object System.Drawing.PointF([single]$x2, [single]$y2)
    return New-Object System.Drawing.Drawing2D.LinearGradientBrush($p1, $p2, $top, $bottom)
}

function Draw-SelectedHighlight([System.Drawing.Graphics]$g) {
    # Keep the white outer canvas. This cyan plate is only a selection halo
    # around the coloured tile, not a full-screen background.
    $path = New-RoundRectPath 2 3 86 42 11
    $brush = New-LinearBrush 2 3 88 45 `
        ([System.Drawing.Color]::FromArgb(0x63, 0xD9, 0xFF)) `
        ([System.Drawing.Color]::FromArgb(0x29, 0xBE, 0xFF))
    $g.FillPath($brush, $path)
    $brush.Dispose()
    $path.Dispose()
}

function Draw-GameTile([System.Drawing.Graphics]$g, [string]$theme) {
    $tx = 5; $ty = 5; $tw = 82; $th = 36; $radius = 9

    if ($theme -eq '2048') {
        $edgeColor = [System.Drawing.Color]::FromArgb(0xB8, 0x3A, 0x27)
        $topColor = [System.Drawing.Color]::FromArgb(0xF6, 0x7A, 0x45)
        $bottomColor = [System.Drawing.Color]::FromArgb(0xE7, 0x4D, 0x32)
    } else {
        $edgeColor = [System.Drawing.Color]::FromArgb(0x2A, 0x7A, 0x35)
        $topColor = [System.Drawing.Color]::FromArgb(0x4F, 0xB2, 0x57)
        $bottomColor = [System.Drawing.Color]::FromArgb(0x3D, 0x91, 0x45)
    }

    # A small lower band gives the tile the same grounded appearance as the
    # official icons while keeping the bottom label area clear.
    $edgePath = New-RoundRectPath $tx ($ty + 2) $tw $th $radius
    $edgeBrush = New-Object System.Drawing.SolidBrush($edgeColor)
    $g.FillPath($edgeBrush, $edgePath)
    $edgeBrush.Dispose()
    $edgePath.Dispose()

    $tilePath = New-RoundRectPath $tx $ty $tw ($th - 2) $radius
    $tileBrush = New-LinearBrush $tx $ty ($tx + $tw) ($ty + $th) $topColor $bottomColor
    $g.FillPath($tileBrush, $tilePath)
    $tileBrush.Dispose()
    $tilePath.Dispose()
}

function Draw-MineSubject([System.Drawing.Graphics]$g) {
    $mx = 46; $my = 23
    $white = [System.Drawing.Color]::White

    $spikePen = New-Object System.Drawing.Pen($white, 4.5)
    $spikePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $spikePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    for ($a = 0; $a -lt 8; $a++) {
        $angle = $a * [math]::PI / 4.0
        $x1 = [single]($mx + 5 * [math]::Cos($angle))
        $y1 = [single]($my + 5 * [math]::Sin($angle))
        $x2 = [single]($mx + 14 * [math]::Cos($angle))
        $y2 = [single]($my + 14 * [math]::Sin($angle))
        $g.DrawLine($spikePen, $x1, $y1, $x2, $y2)
    }
    $spikePen.Dispose()

    $bodyBrush = New-Object System.Drawing.SolidBrush($white)
    $g.FillEllipse($bodyBrush, $mx - 10, $my - 10, 20, 20)
    $bodyBrush.Dispose()

    # A small green centre keeps the white silhouette legible as a mine rather
    # than a generic gear at the calculator's native resolution.
    $centreBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0x3D, 0x91, 0x45))
    $g.FillEllipse($centreBrush, $mx - 2, $my - 2, 4, 4)
    $centreBrush.Dispose()
}

function Draw-2048Subject([System.Drawing.Graphics]$g) {
    $whiteBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $font = New-Object System.Drawing.Font('Arial', 30, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = [System.Drawing.StringAlignment]::Center
    $format.LineAlignment = [System.Drawing.StringAlignment]::Center
    $rect = New-Object System.Drawing.RectangleF(16, 7, 60, 34)
    $g.DrawString('2', $font, $whiteBrush, $rect, $format)
    $format.Dispose()
    $font.Dispose()

    # Short inward chevrons reinforce the game's tile-merging idea without
    # drawing the plugin name into the icon.
    $arrowPen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, 3.0)
    $arrowPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $arrowPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($arrowPen, 28, 24, 35, 24)
    $g.DrawLine($arrowPen, 35, 24, 31, 20)
    $g.DrawLine($arrowPen, 35, 24, 31, 28)
    $g.DrawLine($arrowPen, 64, 24, 57, 24)
    $g.DrawLine($arrowPen, 57, 24, 61, 20)
    $g.DrawLine($arrowPen, 57, 24, 61, 28)
    $arrowPen.Dispose()
    $whiteBrush.Dispose()
}

function New-GameIcon([string]$path, [string]$mode, [string]$theme) {
    $bmp = New-Object System.Drawing.Bitmap(92, 64, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    $g.Clear([System.Drawing.Color]::White)
    if ($mode -eq 'sel') { Draw-SelectedHighlight $g }
    Draw-GameTile $g $theme
    if ($theme -eq '2048') { Draw-2048Subject $g } else { Draw-MineSubject $g }

    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $g.Dispose()
    $bmp.Dispose()
    Write-Output "wrote $path"
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$destinations = @(
    @{ Relative = 'build\minesweeper'; Theme = 'mine' },
    @{ Relative = 'src\casio-minesweeper-main'; Theme = 'mine' },
    @{ Relative = 'build\2048'; Theme = '2048' },
    @{ Relative = 'src\2048-fx-cg-10-20-50-main'; Theme = '2048' }
)

foreach ($destination in $destinations) {
    $dir = Join-Path $root $destination.Relative
    New-GameIcon (Join-Path $dir 'unselected.bmp') 'uns' $destination.Theme
    New-GameIcon (Join-Path $dir 'selected.bmp') 'sel' $destination.Theme
}
