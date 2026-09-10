# =============================================================================
#  Generate Minesweeper add-in icons (92x64, 24bpp RGB BMP) for fx-CG50.
#
#  Pixel-measured style reference (the 2048 add-in icon the user likes):
#    - unselected background: flat white  #FFFFFF
#    - selected   background: flat light sky blue #68D4F8  (NOT a dark gradient)
#    - one rounded tile  x=8 y=9 w=80 h=34  radius~10
#    - tile fill: subtle diagonal gradient (lighter top-left -> darker bottom-right)
#    - tile 3D: a thin (~2px) darker band at the bottom edge only
#    - single bold subject on the tile, plenty of empty space below
#
#  Minesweeper adaptation: green tile (distinct from 2048's orange-red),
#  subject = classic mine ball (dark spiked ball with a glossy highlight).
#
#  ASCII only (PowerShell 5.1 reads scripts as ANSI without BOM).
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

function New-MineIcon([string]$path, [string]$mode) {
    $bmp = New-Object System.Drawing.Bitmap(92, 64, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

    # ---- background: flat white / flat light sky blue (matches 2048) ----
    if ($mode -eq 'sel') {
        $g.Clear([System.Drawing.Color]::FromArgb(0x68, 0xD4, 0xF8))
    } else {
        $g.Clear([System.Drawing.Color]::White)
    }

    # ---- tile geometry (measured to match the 2048 icon exactly) ----
    $tx = 6; $ty = 6; $tw = 80; $th = 34; $rad = 10

    # thin darker bottom edge only (subtle 3D, like 2048's dark bottom band)
    $edgeBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0x2A, 0x7A, 0x35))
    $pEdge = New-RoundRectPath $tx ($ty + 2) $tw $th $rad
    $g.FillPath($edgeBrush, $pEdge)
    $pEdge.Dispose(); $edgeBrush.Dispose()

    # main tile: subtle diagonal gradient (lighter top-left -> darker bottom-right)
    $pTile = New-RoundRectPath $tx $ty $tw $th $rad
    $p1 = New-Object System.Drawing.PointF([single]$tx, [single]$ty)
    $p2 = New-Object System.Drawing.PointF([single]($tx + $tw), [single]($ty + $th))
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $p1, $p2,
        [System.Drawing.Color]::FromArgb(0x4F, 0xB2, 0x57),
        [System.Drawing.Color]::FromArgb(0x3D, 0x91, 0x45))
    $g.FillPath($grad, $pTile)
    $grad.Dispose(); $pTile.Dispose()

    # ---- subject: classic mine ball, centered on the tile ----
    $mx = $tx + [int]($tw / 2)   # 48
    $my = $ty + [int]($th / 2)   # 26

    # 8 short thick spikes in the same dark tone as the ball (classic mine silhouette)
    $dark = [System.Drawing.Color]::FromArgb(0x26, 0x26, 0x26)
    $spikePen = New-Object System.Drawing.Pen($dark, 5.0)
    $spikePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $spikePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    for ($a = 0; $a -lt 8; $a++) {
        $th2 = $a * [math]::PI / 4.0
        $x2 = $mx + 14 * [math]::Cos($th2); $y2 = $my + 14 * [math]::Sin($th2)
        $g.DrawLine($spikePen, [single]$mx, [single]$my, [single]$x2, [single]$y2)
    }
    $spikePen.Dispose()

    # ball body
    $ball = New-Object System.Drawing.SolidBrush($dark)
    $g.FillEllipse($ball, $mx - 10, $my - 10, 20, 20)
    $ball.Dispose()

    # glossy white highlight (top-left of the ball)
    $hl = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.FillEllipse($hl, $mx - 6, $my - 7, 7, 5)
    $hl.Dispose()

    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "wrote $path"
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dir = Join-Path $root 'build\minesweeper'
New-MineIcon (Join-Path $dir 'unselected.bmp') 'uns'
New-MineIcon (Join-Path $dir 'selected.bmp') 'sel'
