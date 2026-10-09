import { FunctionalComponent } from 'preact';
import { route, useRouter } from 'preact-router';
import { t } from '../i18n';
import { tRich } from '../i18n/rich';

const NotFound: FunctionalComponent = () => {
  const [router] = useRouter();
  const currentPath = router.url || window.location.pathname || '/';

  return (
    <div class="not-found-page page-enter">
      <section class="not-found-card">
        <div class="not-found-kicker">
          <span aria-hidden="true">?</span>
          <span>{t('notFound.signalLost')}</span>
        </div>

        <div class="not-found-code">404</div>
        <h2 class="not-found-title">{t('notFound.pageNotFound')}</h2>
        <p>{tRich('notFound.noRoute', { path: <span>{currentPath}</span> })}</p>

        <div class="not-found-actions">
          <button type="button" onClick={() => route('/')}>
            {t('notFound.returnToDashboard')}
          </button>
          <button type="button" onClick={() => route('/system')}>
            {t('notFound.viewSystem')}
          </button>
        </div>
      </section>
    </div>
  );
};

export default NotFound;
