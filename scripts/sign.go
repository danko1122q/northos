package main

import (
	"fmt"
	"os"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintf(os.Stderr, "Usage: sign <file>\n")
		os.Exit(1)
	}

	filename := os.Args[1]
	data, err := os.ReadFile(filename)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading file %s: %v\n", filename, err)
		os.Exit(1)
	}

	n := len(data)
	if n > 510 {
		fmt.Fprintf(os.Stderr, "boot block too large: %d bytes (max 510)\n", n)
		os.Exit(1)
	}

	fmt.Fprintf(os.Stderr, "boot block is %d bytes (max 510)\n", n)

	// Buat buffer 512 byte yang diisi nol
	buf := make([]byte, 512)
	copy(buf, data)

	// Tambahkan signature 0x55AA di akhir
	buf[510] = 0x55
	buf[511] = 0xAA

	err = os.WriteFile(filename, buf, 0644)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error writing file %s: %v\n", filename, err)
		os.Exit(1)
	}
}
