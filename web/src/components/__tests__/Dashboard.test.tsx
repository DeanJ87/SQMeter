import { render, screen } from '@testing-library/preact';
import { describe, it, expect, vi } from 'vitest';
import Dashboard from '../Dashboard';

vi.mock('../../hooks/useWebSocket', () => ({
  useWebSocket: vi.fn(),
}));

import { useWebSocket } from '../../hooks/useWebSocket';

const mockSensorData = {
  lightSensor: { lux: 0.000234, visible: 123, infrared: 45, full: 168, status: 0 },
  environment: { temperature: 22.5, humidity: 45.2, pressure: 1013.25, dewpoint: 11.3, status: 0 },
  irTemperature: { objectTemp: -18.5, ambientTemp: 21.3, status: 0 },
  skyQuality: { sqm: 21.50, nelm: 6.2, bortle: 2.0, description: 'Typical truly dark site' },
  cloudConditions: {
    condition: 1,
    description: 'Clear',
    temperatureDelta: -39.8,
    correctedDelta: -40.2,
    cloudCoverPercent: 0.0,
    humidityUsed: 45.2,
  },
};

describe('Dashboard', () => {
  it('shows connecting state when not connected', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: null, connected: false });
    render(<Dashboard />);
    expect(screen.getByText('Connecting...')).toBeTruthy();
  });

  it('shows loading state when connected but no data', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: null, connected: true });
    render(<Dashboard />);
    expect(screen.getByText('Loading...')).toBeTruthy();
  });

  it('shows sun icon for clear sky (condition 1)', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: mockSensorData, connected: true });
    render(<Dashboard />);
    // Condition 1 renders the sun/clear icon ☀️
    expect(screen.getByText('☀️')).toBeTruthy();
  });

  it('shows cloudy icon for condition 2', () => {
    const cloudyData = {
      ...mockSensorData,
      cloudConditions: {
        ...mockSensorData.cloudConditions,
        condition: 2,
        description: 'Cloudy',
        cloudCoverPercent: 60.0,
      },
    };
    vi.mocked(useWebSocket).mockReturnValue({ data: cloudyData, connected: true });
    render(<Dashboard />);
    expect(screen.getByText('⛅')).toBeTruthy();
  });

  it('shows overcast icon for condition 3', () => {
    const overcastData = {
      ...mockSensorData,
      cloudConditions: {
        ...mockSensorData.cloudConditions,
        condition: 3,
        description: 'Overcast',
        cloudCoverPercent: 100.0,
      },
    };
    vi.mocked(useWebSocket).mockReturnValue({ data: overcastData, connected: true });
    render(<Dashboard />);
    expect(screen.getByText('☁️')).toBeTruthy();
  });

  it('displays cloud cover percentage', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: mockSensorData, connected: true });
    render(<Dashboard />);
    expect(screen.getByText('0%')).toBeTruthy();
  });

  it('sky quality SQM value is displayed', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: mockSensorData, connected: true });
    render(<Dashboard />);
    expect(screen.getByText('21.50')).toBeTruthy();
  });

  it('cloud conditions grid has grid-cols-3 class for responsive layout', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: mockSensorData, connected: true });
    const { container } = render(<Dashboard />);
    // The cloud conditions stats grid uses grid-cols-3
    const grid = container.querySelector('.grid-cols-3');
    expect(grid).toBeTruthy();
  });

  it('sky quality description is displayed', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: mockSensorData, connected: true });
    render(<Dashboard />);
    expect(screen.getByText('Typical truly dark site')).toBeTruthy();
  });

  it('shows connected status indicator when connected', () => {
    vi.mocked(useWebSocket).mockReturnValue({ data: mockSensorData, connected: true });
    render(<Dashboard />);
    expect(screen.getByText('Connected')).toBeTruthy();
  });
});
