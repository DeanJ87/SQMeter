import { FunctionalComponent } from 'preact';
import { route, useRouter } from 'preact-router';

const NotFound: FunctionalComponent = () => {
  const [router] = useRouter();
  const currentPath = router.url || window.location.pathname || '/';

  return (
    <div class="not-found-page page-enter">
      <section class="not-found-card">
        <div class="not-found-kicker">
          <span aria-hidden="true">?</span>
          <span>Signal Lost</span>
        </div>

        <div class="not-found-code">404</div>
        <h1>Page not found</h1>
        <p>
          The dashboard does not have a route for <span>{currentPath}</span>.
        </p>

        <div class="not-found-actions">
          <button type="button" onClick={() => route('/')}>
            Return to Dashboard
          </button>
          <button type="button" onClick={() => route('/system')}>
            View System
          </button>
        </div>
      </section>
    </div>
  );
};

export default NotFound;
