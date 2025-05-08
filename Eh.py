def combine_mif_dual_output(mif_file, hex_output, bin_output):
    hex_lines = []
    bin_lines = []
    buffer = []
    base_addr = None

    with open(mif_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip()

            # Pass header to both files
            if line.startswith(("WIDTH", "DEPTH", "ADDRESS_RADIX", "CONTENT", "BEGIN")):
                hex_lines.append(line)
                bin_lines.append(line)
                continue

            if "DATA_RADIX" in line:
                hex_lines.append("DATA_RADIX = HEX;")
                bin_lines.append("DATA_RADIX = BIN;")
                continue

            if line.startswith("END"):
                if buffer:
                    hex_str = "".join(buffer)
                    bin_str = "".join([f"{int(x, 16):016b}" for x in buffer])
                    hex_lines.append(f"{base_addr} : {hex_str} ;")
                    bin_lines.append(f"{base_addr} : {bin_str} ;")
                hex_lines.append("END;")
                bin_lines.append("END;")
                break

            if ":" in line and ";" in line:
                addr, data = line.split(":")
                addr = addr.strip()
                data = data.replace(";", "").strip()

                if len(buffer) == 0:
                    base_addr = addr

                buffer.append(data)

                if len(buffer) == 8:
                    hex_str = "".join(buffer)
                    bin_str = "".join([f"{int(x, 16):016b}" for x in buffer])
                    hex_lines.append(f"{base_addr} : {hex_str} ;")
                    bin_lines.append(f"{base_addr} : {bin_str} ;")
                    buffer = []
                    base_addr = None
            else:
                hex_lines.append(line)
                bin_lines.append(line)

    with open(hex_output, "w", encoding="utf-8") as f:
        f.write("\n".join(hex_lines))

    with open(bin_output, "w", encoding="utf-8") as f:
        f.write("\n".join(bin_lines))

    print(f"Saved hex output: {hex_output}")
    print(f"Saved binary output: {bin_output}")
