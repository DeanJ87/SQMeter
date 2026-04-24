#!/usr/bin/env python3
"""
Build script to compile TypeScript/Preact web UI and copy to data folder
"""
import os
import shutil
import subprocess
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
    
    # Copy build output to data folder
    dist_dir = web_dir / 'dist'
    if dist_dir.exists():
        # Clear existing data folder
        if data_dir.exists():
            print(f"🗑️  Clearing old data folder...")
            shutil.rmtree(data_dir)
        
        # Copy new build
        print(f"📋 Copying {dist_dir} → {data_dir}")
        shutil.copytree(dist_dir, data_dir)
        
        # List copied files
        files = list(data_dir.rglob('*'))
        print(f"✅ Web UI built successfully! Copied {len(files)} files:")
        for f in sorted(files):
            if f.is_file():
                size_kb = f.stat().st_size / 1024
                print(f"   - {f.relative_to(data_dir)} ({size_kb:.1f} KB)")
        print("=" * 60)
    else:
        print("❌ Build failed: dist folder not found")
        raise Exception("Web UI build failed")

# Hook into both buildfs and uploadfs targets
env.AddPreAction("buildfs", build_web_ui)
env.AddPreAction("uploadfs", build_web_ui)
