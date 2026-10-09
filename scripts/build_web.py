#!/usr/bin/env python3
"""
Build script: compile the TypeScript/Preact web UI and pack it into the data folder
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")

def build_web_ui(*args, **kwargs):
    """Build the web UI and copy to data folder"""
    project_dir = Path(env['PROJECT_DIR'])
    web_dir = project_dir / 'web'
    data_dir = project_dir / 'data'
    
    if not web_dir.exists():
        print("⚠️  Web directory not found, skipping UI build")
        return
    
    print("=" * 60)
    print("🔨 Building Web UI...")
    print("=" * 60)
    
    # Install dependencies if needed
    node_modules = web_dir / 'node_modules'
    if not node_modules.exists():
        print("📦 Installing npm dependencies...")
        subprocess.run(['npm', 'install'], cwd=web_dir, check=True)
    
    # Build the UI
    print("⚙️  Compiling TypeScript and bundling...")
    result = subprocess.run(['npm', 'run', 'build'], cwd=web_dir, check=True)
    
    # Pack the build into the data folder: text files gzipped, demo-only
    # files left out (tools/ui/pack_data.py, coding standard SIZE-02).
    dist_dir = web_dir / 'dist'
    if not dist_dir.exists():
        print("❌ Build failed: dist folder not found")
        raise Exception("Web UI build failed")
    sys.path.insert(0, str(project_dir / 'tools' / 'ui'))
    import pack_data
    print(f"📋 Packing {dist_dir} → {data_dir}")
    stored = pack_data.pack(dist_dir, data_dir)
    source = sum(s for _, s, _ in stored)
    packed = sum(p for _, _, p in stored)
    for path, _, size in stored:
        print(f"   - {path} ({size / 1024:.1f} KB)")
    print(f"✅ Web UI packed: {len(stored)} files, {source / 1024:.1f} KB → {packed / 1024:.1f} KB")
    print("=" * 60)

# Build the UI as soon as a filesystem target is requested, before SCons
# plans the build. A pre-action on the "buildfs"/"uploadfs" aliases runs only
# after littlefs.bin has already been packed from the old data/ folder, so
# uploads shipped the previous web UI.
FS_TARGETS = {"buildfs", "uploadfs", "uploadfsota"}
if FS_TARGETS.intersection(COMMAND_LINE_TARGETS):
    build_web_ui()
