Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$Screen = [System.Windows.Forms.Screen]::PrimaryScreen
$Bitmap = New-Object System.Drawing.Bitmap($Screen.Bounds.Width, $Screen.Bounds.Height)
$Graphic = [System.Drawing.Graphics]::FromImage($Bitmap)
$Graphic.CopyFromScreen($Screen.Bounds.X, $Screen.Bounds.Y, 0, 0, $Screen.Bounds.Size)
# Save to project root tmp folder
$Bitmap.Save('c:\dev\bb3d\tmp\planet_high_res_moon.png')
