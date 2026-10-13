import { FunctionalComponent } from 'preact';

// The SQMeter mark, flat version: the brand's choice for 17-39 px
// (brand/README.md). Decorative next to the "SQMeter" heading, so hidden from
// assistive technology. Source: brand/src/logo/sqmeter-mark-flat.svg.
const BrandMark: FunctionalComponent<{ size?: number }> = ({ size = 32 }) => (
  <svg class="brand-mark" width={size} height={size} viewBox="0 0 24 24" aria-hidden="true" focusable="false">
    <defs>
      <clipPath id="sqm-mark-clip">
        <rect x="2.25" y="2.25" width="19.5" height="19.5" rx="2.75" />
      </clipPath>
    </defs>
    <g clip-path="url(#sqm-mark-clip)">
      <rect x="2.25" y="2.25" width="19.5" height="19.5" fill="#1d2f41" />
      <circle cx="5.2" cy="5.6" r=".6" fill="#8b9baa" />
      <circle cx="9.4" cy="4.6" r=".6" fill="#8b9baa" />
      <path d="M12.8 13.1L13.67 3.14A10 10 0 0 1 20.99 7.36Z" fill="#55c7f2" />
      <path d="M2.25 19.7Q7.5 18.75 12 19.35T21.75 18.9V21.75H2.25Z" fill="#13212f" />
      <ellipse cx="7.1" cy="12.45" rx="1.5" ry="1.7" transform="rotate(14 7.1 12.45)" fill="#060b11" />
      <path d="M6.45 13.8H7.55V15.5H6.45Z" fill="#060b11" />
      <path d="M5.25 21.75L4.5 17.1Q4.3 15.5 5.8 15.2H8.2Q9.6 15.4 9.5 16.9L8.75 21.75Z" fill="#060b11" />
      <path
        d="M9 15.9L10.75 16.15L12.15 13.85"
        fill="none"
        stroke="#060b11"
        stroke-width="1.35"
        stroke-linecap="round"
        stroke-linejoin="round"
      />
      <path d="M11.85 14.15L12.8 13.1" fill="none" stroke="#060b11" stroke-width="1.1" stroke-linecap="round" />
    </g>
  </svg>
);

export default BrandMark;
