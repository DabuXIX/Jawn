def combine_mif_8words(mif_file, output):
    combined_lines = []
    buffer = []
    base_addr = None

    with open(mif_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip()

            if line.startswith(("WIDTH", "DEPTH", "ADDRESS_RADIX", "DATA_RADIX", "CONTENT", "BEGIN")):
                combined_lines.append(line)
                continue

            if line.startswith("END"):
                if buffer:
                    # Write remaining buffered values
                    data_str = "".join(buffer)
                    combined_lines.append(f"{base_addr} : {data_str} ;")
                combined_lines.append("END;")
                break

            if ":" in line and ";" in line:
                parts = line.split(":")
                addr = parts[0].strip()
                data = parts[1].replace(";", "").strip()

                if len(buffer) == 0:
                    base_addr = addr

                buffer.append(data)

                if len(buffer) == 8:
                    data_str = "".join(buffer)
                    combined_lines.append(f"{base_addr} : {data_str} ;")
                    buffer = []
                    base_addr = None
            else:
                combined_lines.append(line)

    with open(output, "w", encoding="utf-8") as out:
        out.write("\n".join(combined_lines))

    print(f"File saved: {output}")
    return output
