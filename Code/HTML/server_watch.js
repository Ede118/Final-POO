(function () {
  const TERMINATION_PATH = '/server_terminated.html';
  const HEALTH_ENDPOINT = '/health.txt';
  const POLL_INTERVAL_MS = 45000;
  if (window.location.pathname.endsWith('server_terminated.html')) {
    return;
  }

  function resolveServerIP() {
    const stored = (sessionStorage.getItem('server_ip') || '').trim();
    if (stored.length) return stored;
    const host = window.location.hostname;
    if (!host || host === 'localhost' || host === '127.0.0.1') {
      return 'localhost';
    }
    return host;
  }

  const serverIp = resolveServerIP();
  const baseUrl = `http://${serverIp}:8080`;
  let redirecting = false;

  function redirectToTermination() {
    if (redirecting) return;
    redirecting = true;
    const target = `${window.location.origin}${TERMINATION_PATH}`;
    window.location.href = target;
  }

  async function pollServer() {
    try {
      const response = await fetch(
        `${baseUrl}${HEALTH_ENDPOINT}?ts=${Date.now()}`,
        { cache: 'no-store' }
      );
      if (response.status === 503 || response.headers.get('X-Server-Closing') === 'yes') {
        redirectToTermination();
      }
    } catch (err) {
      redirectToTermination();
    }
  }

  pollServer();
  window.addEventListener('beforeunload', () => {
    redirecting = true;
  });
  window.setInterval(pollServer, POLL_INTERVAL_MS);
})();
