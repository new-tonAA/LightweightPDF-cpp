# Lightweight PDF Tools in C++

This repository provides **lightweight PDF utilities written in pure C++**, with **no external libraries** required.  
It demonstrates how to work with the PDF format at a very basic level, focusing on **simplicity** and **minimal dependencies**.

## Features

- 🚀 **No external libraries** — only standard C++ is used  
- 📄 **Generate PDF** from scratch (`make_pdf.cpp`)  
- 📚 **Merge two PDFs** into a single file (`merge_simple_pdfs.cpp`)  
- 🪶 Lightweight and educational — good for learning how PDF structure works  
- ⚠️ **Basic only** — these examples cover fundamental PDF operations, not advanced features like compression, or annotations  

## Files

- `make_pdf.cpp`  
  - Creates a minimal, valid PDF file with simple text output  

- `merge_simple_pdfs.cpp`  
  - Demonstrates a simple way to combine two PDF files into one   

## Usage

Compile the examples with a standard C++ compiler:

```bash
g++ make_pdf.cpp -o make_pdf
./make_pdf

g++ merge_simple_pdfs.cpp -o merge_pdfs
./merge_pdfs file1.pdf file2.pdf output.pdf
````

## Notes

* These are **basic demonstrations** of PDF structure and manipulation
* You may **NOT** create two pages when creating a new PDF file (if I have some free time, I will try to fix the problem)
* Not suitable for production or advanced PDF editing
* Goal: **show how PDFs can be created/combined in the most lightweight way possible**

---

✨ If you find this useful, feel free to star the repo and experiment with the code!
