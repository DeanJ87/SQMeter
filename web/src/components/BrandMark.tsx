import { FunctionalComponent } from 'preact';

// The SQMeter mark, detailed version: the brand's choice from 40 px up
// (brand/README.md). The header shows it at 44 px, 40 px on phones.
// Decorative next to the "SQMeter" heading, so hidden from assistive
// technology. Source: brand/src/logo/sqmeter-mark.svg.
const BrandMark: FunctionalComponent = () => (
  <img class="brand-mark" src="/brand-mark.svg" width={44} height={44} alt="" aria-hidden="true" />
);

export default BrandMark;
