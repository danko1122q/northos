import os
from PIL import Image

ICON_SIZE = 30
ICON_NUMBER = 5
ICON_NAMES = ["ICON_CLOSE", "ICON_MINIMIZE", "ICON_WINDOWS", "ICON_FOLDER", "ICON_CHESS"]
ICON_DIR = "icon"
ICON_FILES = {
    "ICON_CLOSE": "close.png",
    "ICON_MINIMIZE": "minus.png",
    "ICON_WINDOWS": "logo.png",
    "ICON_FOLDER": "logo.png",
    "ICON_CHESS": "logo.png"
}
OUTPUT_FILE = "kernel/icons_data.c"
TRANSPARENT_MARKER = "0xFF000000"

def convert():
    print(f"Memproses ikon dari folder: {ICON_DIR}")
    if not os.path.exists(ICON_DIR):
        os.makedirs(ICON_DIR)
        print(f"Folder {ICON_DIR} dibuat. Masukkan file .png ke sana.")
        return

    with open(OUTPUT_FILE, "w") as f:
        f.write('#include "icons.h"\n\n')
        f.write(f'unsigned int icons_data[ICON_NUMBER][ICON_SIZE * ICON_SIZE] = {{\n')

        for name in ICON_NAMES:
            file_name = ICON_FILES.get(name)
            path = os.path.join(ICON_DIR, file_name)
            
            if not os.path.exists(path):
                print(f"Peringatan: {path} tidak ditemukan, menggunakan logo.png sebagai fallback.")
                path = os.path.join(ICON_DIR, "logo.png")

            # Load image dan pastikan RGBA
            img = Image.open(path).resize((ICON_SIZE, ICON_SIZE), Image.Resampling.LANCZOS).convert("RGBA")
            pixels = img.load()

            f.write(f'    [{name}] = {{\n        ')
            
            for y in range(ICON_SIZE):
                for x in range(ICON_SIZE):
                    r, g, b, a = pixels[x, y]
                    
                    # Jika alpha kurang dari 200 (pixel hampir transparan), paksa transparan
                    if a < 200:
                        f.write(f"{TRANSPARENT_MARKER}, ")
                    else:
                        f.write(f"0x00{r:02X}{g:02X}{b:02X}, ")
                f.write("\n        ")
            f.write('\n    },\n')

        f.write('};\n')
    print("Selesai! icons_data.c siap digunakan.")

if __name__ == "__main__":
    convert()