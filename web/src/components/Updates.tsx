import { FunctionalComponent } from 'preact';
import { useState, useEffect } from 'preact/hooks';

type UpdateType = 'firmware' | 'filesystem';

const Updates: FunctionalComponent = () => {
  const [updateType, setUpdateType] = useState<UpdateType>('firmware');
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [uploadProgress, setUploadProgress] = useState(0);
  const [status, setStatus] = useState<string>('');
  const [waitingForReboot, setWaitingForReboot] = useState(false);

  useEffect(() => {
    let checkInterval: number | undefined;
    
    if (waitingForReboot) {
      setStatus('Device is rebooting... waiting for it to come back online');
      
      // Start checking if device is back online
      checkInterval = window.setInterval(async () => {
        try {
          const response = await fetch('/api/status');
          if (response.ok) {
            setStatus('✅ Update successful! Device is back online.');
            setWaitingForReboot(false);
            setFile(null);
            window.clearInterval(checkInterval);
          }
        } catch {
          // Still offline, keep waiting
        }
      }, 2000);
    }

    return () => {
      if (checkInterval) {
        window.clearInterval(checkInterval);
      }
    };
  }, [waitingForReboot]);

  const handleUpload = async () => {
    if (!file) return;

    setUploading(true);
    setUploadProgress(0);
    setStatus('Uploading...');

    const endpoint = updateType === 'firmware' ? '/api/update' : '/api/update/fs';

    // Demo mode: MSW can't trigger XHR upload progress events, so fake it
    if (import.meta.env.VITE_DEMO_MODE === 'true') {
      let p = 0;
      const iv = setInterval(() => {
        p = Math.min(p + Math.random() * 12 + 6, 100);
        const rounded = Math.round(p);
        setUploadProgress(rounded);
        setStatus(`Uploading... ${rounded}%`);
        if (p >= 100) {
          clearInterval(iv);
          setStatus('Upload complete! Device is rebooting...');
          setUploading(false);
          setWaitingForReboot(true);
        }
      }, 250);
      return;
    }

    const formData = new FormData();
    formData.append('update', file);

    try {
      const xhr = new XMLHttpRequest();

      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
          const progress = Math.round((e.loaded / e.total) * 100);
          setUploadProgress(progress);
          setStatus(`Uploading... ${progress}%`);
        }
      });

      xhr.addEventListener('load', () => {
        if (xhr.status === 200) {
          try {
            const response = JSON.parse(xhr.responseText);
            if (response.success === true) {
              setStatus('Upload complete! Device is rebooting...');
              setUploading(false);
              setWaitingForReboot(true);
            } else {
              const errorMsg = response.error || 'Unknown error';
              setStatus(`❌ Upload failed: ${errorMsg}`);
              setUploading(false);
            }
          } catch (e) {
            setStatus('Upload complete! Device is rebooting...');
            setUploading(false);
            setWaitingForReboot(true);
          }
        } else {
          setStatus(`❌ Upload failed: HTTP ${xhr.status}`);
          setUploading(false);
        }
      });

      xhr.addEventListener('error', () => {
        if (uploadProgress === 100) {
          setStatus('Upload complete! Device is rebooting...');
          setUploading(false);
          setWaitingForReboot(true);
        } else {
          setStatus('❌ Upload error occurred');
          setUploading(false);
        }
      });

      xhr.open('POST', endpoint);
      xhr.send(formData);
    } catch (error) {
      setStatus(`❌ Failed to upload: ${error}`);
      setUploading(false);
    }
  };

  const getFileHelp = () => {
    if (updateType === 'firmware') {
      return {
        title: 'Firmware Update',
        description: 'Updates the C++ application code (sensors, cloud detection, calculations)',
        filename: 'firmware.bin',
        location: '.pio/build/esp32dev/firmware.bin'
      };
    } else {
      return {
        title: 'Filesystem Update',
        description: 'Updates the web interface files (HTML, CSS, JavaScript)',
        filename: 'littlefs.bin',
        location: '.pio/build/esp32dev/littlefs.bin'
      };
    }
  };

  const help = getFileHelp();

  return (
    <div class="max-w-2xl mx-auto space-y-6">
      <div>
        <h1 class="text-3xl font-bold text-white mb-2">OTA Updates</h1>
        <p class="text-gray-400">
          Upload firmware or filesystem updates remotely over-the-air
        </p>
      </div>

      {/* Update Type Selection */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">Update Type</h2>
        <div class="space-y-3">
          <label class="flex items-start p-4 bg-gray-700 rounded-lg cursor-pointer hover:bg-gray-650 transition-colors">
            <input
              type="radio"
              name="updateType"
              value="firmware"
              checked={updateType === 'firmware'}
              onChange={() => setUpdateType('firmware')}
              disabled={uploading || waitingForReboot}
              class="mt-1 mr-3"
            />
            <div class="flex-1">
              <div class="text-white font-semibold mb-1">🔧 Firmware (Backend)</div>
              <div class="text-sm text-gray-400">
                Updates C++ application code: sensors, cloud detection, calculations, WiFi, MQTT
              </div>
              <div class="text-xs text-gray-500 mt-1 font-mono">
                File: firmware.bin (~1MB)
              </div>
            </div>
          </label>

          <label class="flex items-start p-4 bg-gray-700 rounded-lg cursor-pointer hover:bg-gray-650 transition-colors">
            <input
              type="radio"
              name="updateType"
              value="filesystem"
              checked={updateType === 'filesystem'}
              onChange={() => setUpdateType('filesystem')}
              disabled={uploading || waitingForReboot}
              class="mt-1 mr-3"
            />
            <div class="flex-1">
              <div class="text-white font-semibold mb-1">🌐 Filesystem (Frontend)</div>
              <div class="text-sm text-gray-400">
                Updates web interface files: HTML, CSS, JavaScript, UI components
              </div>
              <div class="text-xs text-gray-500 mt-1 font-mono">
                File: littlefs.bin (~200KB)
              </div>
            </div>
          </label>
        </div>
      </section>

      {/* File Upload */}
      <section class="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <h2 class="text-xl font-semibold text-white mb-4">{help.title}</h2>
        
        <div class="bg-blue-900/30 border border-blue-700/50 rounded-lg p-4 mb-4">
          <div class="text-blue-200 text-sm">
            <div class="font-semibold mb-1">📋 What this updates:</div>
            <div class="text-blue-300">{help.description}</div>
            <div class="mt-2 text-xs text-blue-400 font-mono">
              Build location: {help.location}
            </div>
          </div>
        </div>

        <div class="space-y-4">
          <div>
            <label class="block text-sm font-medium text-gray-300 mb-2">
              Select {help.filename} file
            </label>
            <input
              type="file"
              accept=".bin"
              onChange={(e) => {
                const files = (e.target as HTMLInputElement).files;
                setFile(files ? files[0] : null);
                setStatus('');
              }}
              disabled={uploading || waitingForReboot}
              class="w-full px-4 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white focus:outline-none focus:border-blue-500 disabled:opacity-50"
            />
            {file && (
              <div class="mt-2 text-sm text-gray-400">
                Selected: {file.name} ({(file.size / 1024).toFixed(1)} KB)
              </div>
            )}
          </div>

          {uploading && (
            <div>
              <div class="flex justify-between text-sm text-gray-400 mb-2">
                <span>Uploading...</span>
                <span>{uploadProgress}%</span>
              </div>
              <div class="w-full bg-gray-700 rounded-full h-3">
                <div
                  class={`h-3 rounded-full transition-all ${
                    updateType === 'firmware' ? 'bg-purple-600' : 'bg-green-600'
                  }`}
                  style={{ width: `${uploadProgress}%` }}
                />
              </div>
            </div>
          )}

          {status && (
            <div class={`p-3 rounded-lg text-sm ${
              status.includes('✅') 
                ? 'bg-green-900/30 border border-green-700/50 text-green-200'
                : status.includes('❌')
                ? 'bg-red-900/30 border border-red-700/50 text-red-200'
                : 'bg-blue-900/30 border border-blue-700/50 text-blue-200'
            }`}>
              {status}
            </div>
          )}

          <button
            onClick={handleUpload}
            disabled={!file || uploading || waitingForReboot}
            class={`w-full px-6 py-3 text-white font-semibold rounded-lg transition-colors disabled:bg-gray-600 disabled:cursor-not-allowed ${
              updateType === 'firmware'
                ? 'bg-purple-600 hover:bg-purple-700'
                : 'bg-green-600 hover:bg-green-700'
            }`}
          >
            {uploading ? 'Uploading...' : waitingForReboot ? 'Waiting for reboot...' : `Upload ${help.title}`}
          </button>
        </div>
      </section>

      {/* Warnings */}
      <section class="bg-yellow-900/20 border border-yellow-700/50 rounded-lg p-4">
        <h3 class="text-yellow-200 font-semibold mb-2">⚠️ Important Notes</h3>
        <ul class="text-sm text-yellow-300 space-y-1 list-disc list-inside">
          <li>Do not power off the device during update</li>
          <li>Do not close this browser tab until update completes</li>
          <li>Device will automatically reboot after successful upload</li>
          <li>For complete updates: upload firmware first, then filesystem after reboot</li>
          <li>Keep backup of working .bin files before updating</li>
        </ul>
      </section>
    </div>
  );
};

export default Updates;
