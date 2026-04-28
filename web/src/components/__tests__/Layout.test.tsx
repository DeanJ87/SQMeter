import { render, screen } from '@testing-library/preact';
import { describe, it, expect, vi } from 'vitest';
import Layout from '../Layout';

vi.mock('preact-router', () => ({
  route: vi.fn(),
}));

describe('Layout', () => {
  it('renders the SQMeter header title', () => {
    render(<Layout currentPath="/">content</Layout>);
    expect(screen.getByText('SQMeter')).toBeTruthy();
  });

  it('renders all 4 navigation items', () => {
    render(<Layout currentPath="/">content</Layout>);
    expect(screen.getByText('Dashboard')).toBeTruthy();
    expect(screen.getByText('System')).toBeTruthy();
    expect(screen.getByText('Settings')).toBeTruthy();
    expect(screen.getByText('Updates')).toBeTruthy();
  });

  it('renders children content', () => {
    render(<Layout currentPath="/">hello world</Layout>);
    expect(screen.getByText('hello world')).toBeTruthy();
  });

  it('renders Dark Sky Monitor subtitle', () => {
    render(<Layout currentPath="/">content</Layout>);
    expect(screen.getByText('Dark Sky Monitor')).toBeTruthy();
  });

  it('highlights active navigation item based on currentPath', () => {
    const { container } = render(<Layout currentPath="/system">content</Layout>);
    // The active button has a border-b-2 border-blue-500 class applied
    const activeButton = container.querySelector('.border-blue-500');
    expect(activeButton).toBeTruthy();
    expect(activeButton?.textContent).toContain('System');
  });

  it('renders footer with project description', () => {
    render(<Layout currentPath="/">content</Layout>);
    expect(screen.getByText(/ESP32 Dark Sky Monitoring System/)).toBeTruthy();
  });
});
