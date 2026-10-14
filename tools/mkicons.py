# Generuje ikony stanu (przy zegarze) z logo SignBridge: app-ok/warn/off.ico = app.ico + kropka stanu w rogu.
# Uruchom w src/panel/res/:  python3 ../../../tools/mkicons.py  (wymaga Pillow)
# app.ico, app-dark.ico (nagłówek panelu) i company.ico pochodzą z paczki logo — nie są generowane.
from PIL import Image, ImageDraw

SIZES = [256, 128, 64, 48, 32, 24, 16]
BADGES = {"app-ok.ico": (46, 160, 67, 255), "app-warn.ico": (232, 168, 32, 255), "app-off.ico": (140, 148, 160, 255)}

src = Image.open("app.ico")

def frame(size, color):
    src.size = (size, size)
    img = src.copy().convert("RGBA")
    d = ImageDraw.Draw(img)
    u = size / 100.0
    r = 19 * u
    bx, by = size - r - 2 * u, size - r - 2 * u
    d.ellipse([bx - r - 4 * u, by - r - 4 * u, bx + r + 4 * u, by + r + 4 * u], fill=(255, 255, 255, 255))
    d.ellipse([bx - r, by - r, bx + r, by + r], fill=color)
    return img

for name, color in BADGES.items():
    frames = [frame(s, color) for s in SIZES]
    frames[0].save(name, format="ICO", sizes=[(s, s) for s in SIZES], append_images=frames[1:])
print("ok")
