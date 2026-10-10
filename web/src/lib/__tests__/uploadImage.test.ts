import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { UPLOAD_STALL_MS, uploadImage } from '../updates';

// A minimal XMLHttpRequest that lets the test drive upload progress.
class FakeXhr {
  static last: FakeXhr;
  upload = new EventTarget();
  events = new EventTarget();
  aborted = false;
  status = 0;
  responseText = '';
  constructor() {
    FakeXhr.last = this;
  }
  addEventListener(type: string, listener: EventListener) {
    this.events.addEventListener(type, listener);
  }
  open() {}
  send() {}
  abort() {
    this.aborted = true;
  }
  progress(loaded: number, total: number) {
    const e = Object.assign(new Event('progress'), { lengthComputable: true, loaded, total });
    this.upload.dispatchEvent(e);
  }
  finish(status: number, text: string) {
    this.upload.dispatchEvent(new Event('load'));
    this.status = status;
    this.responseText = text;
    this.events.dispatchEvent(new Event('load'));
  }
}

const handlers = () => ({ onProgress: vi.fn(), onLoad: vi.fn(), onError: vi.fn(), onStall: vi.fn() });
const file = new File([new Uint8Array(10)], 'firmware.bin');

describe('uploadImage', () => {
  beforeEach(() => {
    vi.useFakeTimers();
    vi.stubGlobal('XMLHttpRequest', FakeXhr);
  });
  afterEach(() => {
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  it('aborts and reports a stall when nothing is sent for the stall time', () => {
    const h = handlers();
    uploadImage('/api/update', file, h);
    FakeXhr.last.progress(10, 100);
    vi.advanceTimersByTime(UPLOAD_STALL_MS + 1);
    expect(FakeXhr.last.aborted).toBe(true);
    expect(h.onStall).toHaveBeenCalledOnce();
  });

  it('keeps waiting while progress arrives', () => {
    const h = handlers();
    uploadImage('/api/update', file, h);
    for (let p = 10; p <= 90; p += 10) {
      vi.advanceTimersByTime(UPLOAD_STALL_MS - 1000);
      FakeXhr.last.progress(p, 100);
    }
    expect(h.onStall).not.toHaveBeenCalled();
    expect(h.onProgress).toHaveBeenLastCalledWith(90);
  });

  it('does not report a stall once the file is sent and the device is replying', () => {
    const h = handlers();
    uploadImage('/api/update', file, h);
    FakeXhr.last.progress(100, 100);
    FakeXhr.last.finish(200, '{"success":true}');
    vi.advanceTimersByTime(UPLOAD_STALL_MS * 3);
    expect(h.onStall).not.toHaveBeenCalled();
    expect(h.onLoad).toHaveBeenCalledWith(200, '{"success":true}');
  });
});
