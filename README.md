# AR 
Ar is archive manager/format for .ARX (Achive X) files.  
  
# Features
- Unix-like interactive terminal.  
- Navigate archive contents like a filesystem.  
- Clean and Minimal codebase.  
- Simple CLI interface.
  
# Usage
```bash
git clone https://git.sr.ht/~oled/ar
cd ar
make
./ar -h
# for example: ./ar -n file.arx 
# -> navigate inside an archive like a file system
```
# Technical Details
- AR is built on [lightfs](https://git.sr.ht/~oled/lightfs).  
- File Extension: .ARX
- Interface: Unix-like command env and CLI.
- Design Model: Archive as navigable filesystem abstraction.
  
# Differences From mult
**[mult](https://git.sr.ht/~oled/mult)** is structurally complex and comparatively more problematic due to its layered and tightlt coupled internal design, whereas **AR** is simpler, lightfs-based and stable.  
  
  
# Differences From TAR
**tar** doesnt allow real-time navigate inside an archive like a file system or does it support deleting or adding files without extraction.  
  
# Author 
Created by **oled**.
