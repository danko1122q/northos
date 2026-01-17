package main

import (
	"bufio"
	"fmt"
	"os"
	"strings"
	"time"
)

func main() {
	if len(os.Args) < 2 {
		os.Exit(0)
	}

	header := os.Args[len(os.Args)-1]
	now := time.Now().Format("Jan _2 15:04 2006")
	
	var lines []string
	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := scanner.Text()
		// Hapus komentar //DOC
		if idx := strings.Index(line, "//DOC"); idx != -1 {
			line = line[:idx]
		}
		lines = append(lines, line)
	}

	page := 0
	for i := 0; i < len(lines); i += 50 {
		page++
		fmt.Printf("\n\n%s  %s  Page %d\n\n\n", now, header, page)
		
		var j int
		for j = i; j < i+50 && j < len(lines); j++ {
			fmt.Println(lines[j])
		}
		for ; j < i+50; j++ {
			fmt.Println()
		}
		
		sheet := ""
		if len(lines[i]) >= 4 && i < len(lines) {
			if strings.Contains(lines[i], " ") {
				parts := strings.Split(lines[i], " ")
				if len(parts[0]) == 4 {
					sheet = "Sheet " + parts[0][:2]
				}
			}
		}
		fmt.Printf("\n\n%s\n\n\n", sheet)
	}
}
