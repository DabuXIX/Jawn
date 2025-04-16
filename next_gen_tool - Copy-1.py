import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageDraw, ImageFont
import numpy as np
import os

def reverse_bits(byte):
    # """Reverse the bits in a single byte (8 bits)."""
    # reversed_byte = 0
    # for i in range(8):
    #     if byte & (1 << i):
    #         reversed_byte |= (1 << (7 - i))
    return byte

def generate_xbm_data(ttf_path, char_list, forced_height, max_width,
                      canvas_width, canvas_height,
                      padding_top=0, padding_bottom=0,
                      grid_width_override=None, grid_height_override=None,
                      threshold_value=128):
    """
    Generates XBM data for characters.
    If canvas_width==32 and canvas_height==64 then by default it uses a grid of 17x39.
    You can override this grid by providing grid_width_override and grid_height_override.
    """
    font_size = forced_height * 2
    font = ImageFont.truetype(ttf_path, font_size)
    all_xbm_data = {}
    
    # Define special cases for punctuation/narrow characters (if needed)
    punctuation_set = {',', '.'}
    punctuation_scale = 0.25
    narrow_chars = {"I"}
    narrow_char_scale = 0.5

    # Determine grid dimensions.
    if canvas_width == 32 and canvas_height == 64:
        if grid_width_override is not None and grid_height_override is not None:
            grid_width = grid_width_override
            grid_height = grid_height_override
        else:
            grid_width = 17
            grid_height = 39
    else:
        grid_width = canvas_width
        grid_height = canvas_height

    for char in char_list:
        try:
            if char == " ":
                # Create an empty grid for a space character.
                xbm_data = [[0x00 for _ in range(canvas_width // 8)]
                            for _ in range(canvas_height)]
                all_xbm_data[char] = xbm_data
                continue

            (width, height), (offset_x, offset_y) = font.font.getsize(char)
            if width == 0 or height == 0:
                continue

            # Render the character into an image.
            image = Image.new('L', (width, height), 0)
            draw = ImageDraw.Draw(image)
            draw.text((-offset_x, -offset_y), char, font=font, fill=255)

            # Set target scaling based on character type.
            if char in punctuation_set:
                target_height = int(forced_height * punctuation_scale)
                aspect_ratio = width / height
                scaled_width = min(int(target_height * aspect_ratio), max_width)
            elif char in narrow_chars:
                target_height = forced_height
                aspect_ratio = width / height
                scaled_width = min(int(target_height * aspect_ratio * narrow_char_scale), max_width)
            else:
                target_height = forced_height
                aspect_ratio = width / height
                scaled_width = min(int(target_height * aspect_ratio), max_width)

            img_resized = image.resize((scaled_width, target_height), Image.Resampling.LANCZOS)
            binary_array = (np.array(img_resized) > threshold_value).astype(np.uint8)

            # Create an empty padded array.
            padded_array = np.zeros((canvas_height, canvas_width), dtype=np.uint8)

            if canvas_width == 32 and canvas_height == 64:
                # For the 32x64 canvas, align using grid dimensions.
                if char in punctuation_set:
                    # Align punctuation to the bottom of the grid.
                    vertical_offset = grid_height - binary_array.shape[0]
                else:
                    vertical_offset = max((grid_height - binary_array.shape[0]) // 2, 0)
                horizontal_offset = max((grid_width - binary_array.shape[1]) // 2, 0)
                padded_array[vertical_offset:vertical_offset + binary_array.shape[0],
                             horizontal_offset:horizontal_offset + binary_array.shape[1]] = binary_array
            else:
                # For non-32x64, use padding values directly.
                vertical_start = padding_top
                horizontal_padding = (canvas_width - scaled_width) // 2
                padded_array[vertical_start:vertical_start + target_height,
                             horizontal_padding:horizontal_padding + scaled_width] = binary_array

            # Convert padded image rows into an array of bytes.
            xbm_data = []
            for row in padded_array:
                row_bytes = []
                for byte_index in range(0, canvas_width, 8):
                    byte = 0
                    for bit_index in range(8):
                        col = byte_index + bit_index
                        if col < canvas_width and row[col]:
                            byte |= (1 << (7 - bit_index))
                    row_bytes.append(reverse_bits(byte))
                xbm_data.append(row_bytes)

            all_xbm_data[char] = xbm_data

        except Exception as e:
            print(f"Warning: Unable to process character '{char}'. Reason: {e}")

    return all_xbm_data

def write_xbm(all_xbm_data, output_file, canvas_width, canvas_height):
    """
    Writes XBM data to a file.
    Strikeout has been removed so only normal character data is output.
    """
    with open(output_file, "w", encoding="utf-8") as f:
        f.write("# XBM File\n\n")
        for char, xbm_data in all_xbm_data.items():
            f.write(f"/* Character: '{char}' */\n")
            f.write(f"#define {char}_width {canvas_width}\n")
            f.write(f"#define {char}_height {canvas_height}\n")
            f.write(f"static char {char}_bits[] = {{\n")
            for row_bytes in xbm_data:
                f.write("  " + ", ".join(f"0x{byte:02X}" for byte in row_bytes) + ",\n")
            f.write("};\n\n")
    print(f"XBM file saved as {output_file}")

def write_mif(all_xbm_data, output_file, canvas_width, canvas_height):
    """
    Writes MIF data to a file (normal version only).
    For 32x64 canvases it writes the MIF normally; later these files will be split into high and low halves.
    """
    output_lines = []
    
    # Write MIF header.
    depth = 8192 if canvas_width == 16 and canvas_height == 32 else 16384
    header = [
        f"DEPTH = {depth};",
        f"WIDTH = {canvas_width};",
        "ADDRESS_RADIX = HEX;",
        "DATA_RADIX = HEX;",
        "CONTENT BEGIN"
    ]
    output_lines.extend(header)
    
    address = 0x0000
    for char, xbm_data in all_xbm_data.items():
        output_lines.append(f"-- Character: '{char}'")
        for row_bytes in xbm_data:
            word = "".join(f"{byte:02X}" for byte in row_bytes)
            output_lines.append(f"{address:04X} : {word};")
            address += 1
    output_lines.append("END;")
    
    with open(output_file, "w", encoding="utf-8") as f:
        f.write("\n".join(output_lines))
    
    print(f"MIF file saved as {output_file}")

def write_combined_binary(mif_16x32_file, 
                          mif_32x64_orig_low_file, mif_32x64_orig_high_file,
                          mif_32x64_new_low_file, mif_32x64_new_high_file,
                          output_file, target_size=73728):
    """
    Combines three sections into a single binary file.
    The sections, in order, are:
      1. 16x32 section (normal, no splitting)
      2. Original 32x64 section (rendered to 17x39) split into high and low halves
      3. New 32x64 section (rendered to 26x58) split into high and low halves
    
    The final file has a total size of 72 KB (73728 bytes) including a 2-byte checksum
    at the very end. Base offsets are defined below.
    """
    def load_mif_data(file_path):
        """Loads MIF data and returns a list of (address, data_str)."""
        entries = []
        with open(file_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if ":" not in line or not line.endswith(";"):
                    continue
                parts = line.split(":")
                address_str = parts[0].strip()
                data_str = parts[1].split(";")[0].strip()
                try:
                    addr = int(address_str, 16)
                except ValueError:
                    continue
                if len(data_str) == 4 and all(c in "0123456789ABCDEFabcdef" for c in data_str):
                    entries.append((addr, data_str))
        return entries

    # Load MIF data for each section.
    data_16x32 = load_mif_data(mif_16x32_file)
    data_32x64_orig_low = load_mif_data(mif_32x64_orig_low_file)
    data_32x64_orig_high = load_mif_data(mif_32x64_orig_high_file)
    data_32x64_new_low = load_mif_data(mif_32x64_new_low_file)
    data_32x64_new_high = load_mif_data(mif_32x64_new_high_file)

    # Define base offsets.
    # We assume:
    #   16x32 section occupies 0x2000 bytes (8192 bytes).
    #   Each 32x64 section is split into two 0x4000-byte blocks (16384 bytes each)
    base_offsets = {
        "16x32": 0x0000,
        "32x64_orig_high": 0x2000,                 # starts immediately after 16x32 section
        "32x64_orig_low": 0x6000,          # high part of original section
        "32x64_new_high": 0xA000,        # new section high starts after original section
        "32x64_new_low": 0xD000,         # new section low follows its high part
    }
    # The data region is target_size - 2 bytes (2 bytes reserved for checksum)
    prefill_size = target_size
    
    # Pre-fill the output binary file.
    with open(output_file, "wb") as bin_file:
        bin_file.write(b"\x00" * prefill_size)
    
    # Now write each section’s data into its allocated region.
    with open(output_file, "r+b") as bin_file:
        # Section 1: 16x32 (normal)
        for addr, data_str in data_16x32:
            offset = base_offsets["16x32"] + (addr * 2)
            if offset < prefill_size:
                bin_file.seek(offset)
                bin_file.write(bytes.fromhex(data_str))
        
        # Section 2: Original 32x64 section (split into high and low)
        for addr, data_str in data_32x64_orig_high:
            offset = base_offsets["32x64_orig_high"] + (addr * 2)
            if offset < prefill_size:
                bin_file.seek(offset)
                bin_file.write(bytes.fromhex(data_str))
        for addr, data_str in data_32x64_orig_low:
            offset = base_offsets["32x64_orig_low"] + (addr * 2)
            if offset < prefill_size:
                bin_file.seek(offset)
                bin_file.write(bytes.fromhex(data_str))
        
        # Section 3: New 32x64 section (26x58), split into high and low
        for addr, data_str in data_32x64_new_high:
            offset = base_offsets["32x64_new_high"] + (addr * 2)
            if offset < prefill_size:
                bin_file.seek(offset)
                bin_file.write(bytes.fromhex(data_str))
        for addr, data_str in data_32x64_new_low:
            offset = base_offsets["32x64_new_low"] + (addr * 2)
            if offset < prefill_size:
                bin_file.seek(offset)
                bin_file.write(bytes.fromhex(data_str))
    
    # Calculate and write the 2-byte checksum at the end.
    def calculate_checksum(file_path, data_size):
        with open(file_path, "rb+") as f:
            data = bytearray(f.read())
            checksum = sum(data[:data_size]) & 0xFFFF  # C++-like little-endian sum
            data[-2] = ((checksum >> 8) & 0xFF)
            data[-1] = (checksum & 0xFF)
            
            f.seek(0)
            f.write(data)
        print(f"Checksum 0x{checksum:04X} stored in last 2 bytes of '{file_path}'.")
    
    calculate_checksum(output_file, prefill_size)
    final_size = os.path.getsize(output_file)
    print(f"Combined binary written to {output_file}")
    print(f"Total file size: {final_size} bytes (expected {target_size})")

# -------------------------------------------------------------------
# GUI and file-generation orchestration
def browse_ttf_path(entry):
    path = filedialog.askopenfilename(filetypes=[("Font Files", "*.ttf;*.ttc")])
    if path:
        entry.delete(0, tk.END)
        entry.insert(0, path)

def browse_output_dir(entry):
    path = filedialog.askdirectory()
    if path:
        entry.delete(0, tk.END)
        entry.insert(0, path)

def generate_files():
    try:
        # Get paths.
        ttf_path = ttf_entry.get()
        output_dir = output_dir_entry.get()
        
        # Get configuration for 32x64 (original) section.
        forced_height_32x64 = int(forced_height_32x64_entry.get())
        max_width_32x64 = int(max_width_32x64_entry.get())
        padding_top_32x64 = int(padding_top_32x64_entry.get())
        padding_bottom_32x64 = int(padding_bottom_32x64_entry.get())
        
        # Get configuration for 16x32 section.
        forced_height_16x32 = int(forced_height_16x32_entry.get())
        max_width_16x32 = int(max_width_16x32_entry.get())
        padding_top_16x32 = int(padding_top_16x32_entry.get())
        padding_bottom_16x32 = int(padding_bottom_16x32_entry.get())
        
        # New 32x64 section: force a grid of 26x58.
        # Here we set forced_height and max_width to force the glyph to be rendered to 58 and 26 respectively.
        forced_height_new = 58
        max_width_new = 26
        # Use the same padding values as for the original 32x64 section (or change if desired).
        padding_top_new = padding_top_32x64
        padding_bottom_new = padding_bottom_32x64
        
        # Validate paths.
        if not os.path.exists(ttf_path):
            messagebox.showerror("Error", "Invalid TTF font path.")
            return
        if not os.path.exists(output_dir):
            messagebox.showerror("Error", "Invalid output directory path.")
            return
        
        # Character list – you can adjust as needed.
        char_list = (
            [chr(i) for i in range(0x20, 0x61)] +  
            [chr(0x7B), chr(0x7C), chr(0x7D), chr(0x7E), 
             chr(0xB0), chr(0xB1), chr(0x2026), chr(0x2190), 
             chr(0x2191), chr(0x2192), chr(0x2193), chr(0x21CC), 
             chr(0x25BC), chr(0x2713), chr(0x20)]
        )
        
        os.makedirs(output_dir, exist_ok=True)
        
        # Generate 16x32 data.
        xbm_data_16x32 = generate_xbm_data(
            ttf_path, char_list,
            forced_height_16x32, max_width_16x32,
            canvas_width=16, canvas_height=32,
            padding_top=padding_top_16x32, padding_bottom=padding_bottom_16x32
        )
        file_16x32_xbm = os.path.join(output_dir, "FontRom32.xbm")
        file_16x32_mif = os.path.join(output_dir, "FontRom32.mif")
        write_xbm(xbm_data_16x32, file_16x32_xbm, 16, 32)
        write_mif(xbm_data_16x32, file_16x32_mif, 16, 32)
        
        # Generate original 32x64 data (17x39 grid as before).
        xbm_data_32x64_orig = generate_xbm_data(
            ttf_path, char_list,
            forced_height_32x64, max_width_32x64,
            canvas_width=32, canvas_height=64,
            padding_top=padding_top_32x64, padding_bottom=padding_bottom_32x64
        )
        file_32x64_orig_xbm = os.path.join(output_dir, "FontRom64.xbm")
        file_32x64_orig_mif = os.path.join(output_dir, "FontRom64.mif")
        write_xbm(xbm_data_32x64_orig, file_32x64_orig_xbm, 32, 64)
        write_mif(xbm_data_32x64_orig, file_32x64_orig_mif, 32, 64)
        
        # Generate new 32x64 section data (with 26x58 inner grid).
        xbm_data_32x64_new = generate_xbm_data(
            ttf_path, char_list,
            forced_height_new, max_width_new,
            canvas_width=32, canvas_height=64,
            padding_top=padding_top_new, padding_bottom=padding_bottom_new,
            grid_width_override=26, grid_height_override=58
        )
        file_32x64_new_xbm = os.path.join(output_dir, "FontRomCustom.xbm")
        file_32x64_new_mif = os.path.join(output_dir, "FontRomCustom.mif")
        write_xbm(xbm_data_32x64_new, file_32x64_new_xbm, 32, 64)
        write_mif(xbm_data_32x64_new, file_32x64_new_mif, 32, 64)
        
        # For the 32x64 sections we need to split the MIF files into high and low halves.
        # For simplicity, here we assume you use external (or additional) logic to split the MIF.
        # For this example, we will expect that you have split files produced as follows:
        #    Original 32x64: FontRom64_High.mif and FontRom64_Low.mif
        #    New section: FontRomCustom_High.mif and FontRomCustom_Low.mif
        # You may split the MIF file by taking each word (which is 4 bytes = 8 hex digits)
        # and then splitting into two halves (first 4 digits for "high", last 4 digits for "low").
        #
        # --- For this example, we assume you have a split_mif function that takes a MIF file and produces two files.
        # (You can adapt the splitting logic from your previous code.)
        #
        # For demonstration, here is a very simple split (it reads the lines and splits words):
        def split_mif(mif_file, high_file, low_file):
            high_lines = []
            low_lines = []
            with open(mif_file, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.rstrip()
                    if line.startswith("--") or line.startswith("DEPTH") or line.startswith("WIDTH") or line.startswith("ADDRESS") or line.startswith("DATA"):
                        high_lines.append(line)
                        low_lines.append(line)
                    elif ":" in line and line.endswith(";"):
                        parts = line.split(":")
                        addr = parts[0].strip()
                        data_str = parts[1].split(";")[0].strip()
                        if len(data_str) == 8:
                            # split into high (first 4 hex digits) and low (last 4 hex digits)
                            high_part = data_str[0:4]
                            low_part = data_str[4:8]
                            high_lines.append(f"{addr} : {high_part};")
                            low_lines.append(f"{addr} : {low_part};")
                        else:
                            high_lines.append(line)
                            low_lines.append(line)
                    else:
                        high_lines.append(line)
                        low_lines.append(line)
                # Append END;
                high_lines.append("END;")
                low_lines.append("END;")
            with open(high_file, "w", encoding="utf-8") as hf:
                hf.write("\n".join(high_lines))
            with open(low_file, "w", encoding="utf-8") as lf:
                lf.write("\n".join(low_lines))
            print(f"Split MIF files saved: {high_file} and {low_file}")
        
        orig_high_mif = os.path.join(output_dir, "FontRom64_High.mif")
        orig_low_mif  = os.path.join(output_dir, "FontRom64_Low.mif")
        new_high_mif = os.path.join(output_dir, "FontRomCustom_High.mif")
        new_low_mif  = os.path.join(output_dir, "FontRomCustom_Low.mif")
        split_mif(file_32x64_orig_mif, orig_high_mif, orig_low_mif)
        split_mif(file_32x64_new_mif, new_high_mif, new_low_mif)
        
        # Generate the combined binary file.
        output_binary_file = os.path.join(output_dir, "FontRomCombined.bin")
        write_combined_binary(
            mif_16x32_file=file_16x32_mif,
            mif_32x64_orig_low_file=orig_low_mif,
            mif_32x64_orig_high_file=orig_high_mif,
            mif_32x64_new_low_file=new_low_mif,
            mif_32x64_new_high_file=new_high_mif,
            output_file=output_binary_file,
            target_size=73728  # 72 KB total including checksum
        )
        
        # Optionally, you can remove temporary XBM and MIF files here.
        files_to_remove = [
            file_16x32_xbm, file_16x32_mif,
            file_32x64_orig_xbm, file_32x64_orig_mif,
            file_32x64_new_xbm, file_32x64_new_mif,
            orig_high_mif, orig_low_mif,
            new_high_mif, new_low_mif
        ]
        for file_path in files_to_remove:
            if os.path.exists(file_path):
                os.remove(file_path)
                
        messagebox.showinfo("Success", "Files and combined binary generated successfully!")
    
    except Exception as e:
        messagebox.showerror("Error", f"An error occurred: {e}")

# -------------------------------------------------------------------
# GUI setup.
root = tk.Tk()
root.title("Font to XBM/MIF Converter (Modified)")

# ------------------------
# 1) Frame for TTF Font and Output Directory
# ------------------------
paths_frame = tk.LabelFrame(root, text="File Paths", padx=10, pady=10)
paths_frame.pack(fill="x", padx=10, pady=10)

# TTF path
ttf_label = tk.Label(paths_frame, text="TTF Font Path:")
ttf_label.grid(row=0, column=0, sticky="e", pady=5)
ttf_entry = tk.Entry(paths_frame, width=50)
ttf_entry.grid(row=0, column=1, padx=5, pady=5, sticky="we")
ttf_browse = tk.Button(paths_frame, text="Browse", command=lambda: browse_ttf_path(ttf_entry))
ttf_browse.grid(row=0, column=2, padx=5, pady=5)

# Output directory
output_dir_label = tk.Label(paths_frame, text="Output Directory:")
output_dir_label.grid(row=1, column=0, sticky="e", pady=5)
output_dir_entry = tk.Entry(paths_frame, width=50)
output_dir_entry.grid(row=1, column=1, padx=5, pady=5, sticky="we")
output_dir_browse = tk.Button(paths_frame, text="Browse", command=lambda: browse_output_dir(output_dir_entry))
output_dir_browse.grid(row=1, column=2, padx=5, pady=5)

# Ensure columns expand as needed
paths_frame.grid_columnconfigure(1, weight=1)

# ------------------------
# 2) Frame for 32×64 (Original) Configuration
# ------------------------
config_32x64_frame = tk.LabelFrame(root, text="32×64 Original Configuration (17×39)", padx=10, pady=10)
config_32x64_frame.pack(fill="x", padx=10, pady=5)

forced_height_32x64_label = tk.Label(config_32x64_frame, text="Forced Height:")
forced_height_32x64_label.grid(row=0, column=0, sticky="e", pady=2)
forced_height_32x64_entry = tk.Entry(config_32x64_frame, width=10)
forced_height_32x64_entry.insert(0, "39")
forced_height_32x64_entry.grid(row=0, column=1, padx=5, pady=2, sticky="w")

max_width_32x64_label = tk.Label(config_32x64_frame, text="Max Width:")
max_width_32x64_label.grid(row=1, column=0, sticky="e", pady=2)
max_width_32x64_entry = tk.Entry(config_32x64_frame, width=10)
max_width_32x64_entry.insert(0, "17")
max_width_32x64_entry.grid(row=1, column=1, padx=5, pady=2, sticky="w")

padding_top_32x64_label = tk.Label(config_32x64_frame, text="Padding Top:")
padding_top_32x64_label.grid(row=2, column=0, sticky="e", pady=2)
padding_top_32x64_entry = tk.Entry(config_32x64_frame, width=10)
padding_top_32x64_entry.insert(0, "0")
padding_top_32x64_entry.grid(row=2, column=1, padx=5, pady=2, sticky="w")

padding_bottom_32x64_label = tk.Label(config_32x64_frame, text="Padding Bottom:")
padding_bottom_32x64_label.grid(row=3, column=0, sticky="e", pady=2)
padding_bottom_32x64_entry = tk.Entry(config_32x64_frame, width=10)
padding_bottom_32x64_entry.insert(0, "2")
padding_bottom_32x64_entry.grid(row=3, column=1, padx=5, pady=2, sticky="w")

config_32x64_frame.grid_columnconfigure(1, weight=1)

# ------------------------
# 3) Frame for 16×32 Configuration
# ------------------------
config_16x32_frame = tk.LabelFrame(root, text="16×32 Configuration", padx=10, pady=10)
config_16x32_frame.pack(fill="x", padx=10, pady=5)

forced_height_16x32_label = tk.Label(config_16x32_frame, text="Forced Height:")
forced_height_16x32_label.grid(row=0, column=0, sticky="e", pady=2)
forced_height_16x32_entry = tk.Entry(config_16x32_frame, width=10)
forced_height_16x32_entry.insert(0, "28")
forced_height_16x32_entry.grid(row=0, column=1, padx=5, pady=2, sticky="w")

max_width_16x32_label = tk.Label(config_16x32_frame, text="Max Width:")
max_width_16x32_label.grid(row=1, column=0, sticky="e", pady=2)
max_width_16x32_entry = tk.Entry(config_16x32_frame, width=10)
max_width_16x32_entry.insert(0, "13")
max_width_16x32_entry.grid(row=1, column=1, padx=5, pady=2, sticky="w")

padding_top_16x32_label = tk.Label(config_16x32_frame, text="Padding Top:")
padding_top_16x32_label.grid(row=2, column=0, sticky="e", pady=2)
padding_top_16x32_entry = tk.Entry(config_16x32_frame, width=10)
padding_top_16x32_entry.insert(0, "2")
padding_top_16x32_entry.grid(row=2, column=1, padx=5, pady=2, sticky="w")

padding_bottom_16x32_label = tk.Label(config_16x32_frame, text="Padding Bottom:")
padding_bottom_16x32_label.grid(row=3, column=0, sticky="e", pady=2)
padding_bottom_16x32_entry = tk.Entry(config_16x32_frame, width=10)
padding_bottom_16x32_entry.insert(0, "2")
padding_bottom_16x32_entry.grid(row=3, column=1, padx=5, pady=2, sticky="w")

config_16x32_frame.grid_columnconfigure(1, weight=1)

# ------------------------
# 4) Frame for the new 32×64 (26×58) section
# ------------------------
config_new_32x64_frame = tk.LabelFrame(root, text="New 32×64 Section (26×58)", padx=10, pady=10)
config_new_32x64_frame.pack(fill="x", padx=10, pady=5)

# If these are fixed in code, you can just display them or make them read-only.
label_info = tk.Label(config_new_32x64_frame, 
    text="Forced Height: 58\nMax Width: 26\n(These are hard-coded in the script.)",
    justify="left")
label_info.pack(anchor="w")

# ------------------------
# 5) Generate Button
# ------------------------
generate_button = tk.Button(root, text="Generate Files", command=generate_files)
generate_button.pack(pady=10)

root.mainloop()