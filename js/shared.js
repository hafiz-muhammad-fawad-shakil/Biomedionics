// ═══ BIOMEDIONICS — Shared Utilities ═══
// Cart management
function getCart() {
  try { return JSON.parse(localStorage.getItem('bio_cart') || '[]'); } catch(e) { return []; }
}
function saveCart(cart) {
  localStorage.setItem('bio_cart', JSON.stringify(cart));
  updateCartBadge();
}
function addToCart(item) {
  const cart = getCart();
  const existing = cart.find(i => i.id === item.id);
  if (existing) { existing.qty += (item.qty || 1); }
  else { cart.push({ ...item, qty: item.qty || 1 }); }
  saveCart(cart);
  showToast('✅ Added to cart!');
}
function updateCartBadge() {
  const cart = getCart();
  const total = cart.reduce((s, i) => s + i.qty, 0);
  document.querySelectorAll('#cartBadge, .cart-badge').forEach(el => {
    el.textContent = total;
    el.style.display = total > 0 ? 'inline-flex' : 'none';
  });
}

// Toast
function showToast(msg, duration) {
  let t = document.getElementById('bioToast');
  if (!t) {
    t = document.createElement('div');
    t.id = 'bioToast';
    t.style.cssText = 'position:fixed;bottom:20px;right:20px;background:#0a1628;color:#fff;padding:11px 18px;border-radius:8px;font-size:13px;font-weight:600;z-index:9999;transform:translateY(60px);opacity:0;transition:all .25s;pointer-events:none;box-shadow:0 4px 16px rgba(0,0,0,.2);font-family:inherit;max-width:320px;';
    document.body.appendChild(t);
  }
  t.textContent = msg;
  t.style.transform = 'translateY(0)';
  t.style.opacity = '1';
  clearTimeout(t._timer);
  t._timer = setTimeout(() => { t.style.transform = 'translateY(60px)'; t.style.opacity = '0'; }, duration || 2800);
}

// Spinner
function showSpinner(title, msg) {
  let s = document.getElementById('bioSpinner');
  if (!s) {
    s = document.createElement('div');
    s.id = 'bioSpinner';
    s.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,.5);display:flex;align-items:center;justify-content:center;z-index:9999;';
    s.innerHTML = '<div style="background:#fff;border-radius:14px;padding:32px 40px;text-align:center;box-shadow:0 20px 60px rgba(0,0,0,.2)"><div style="font-size:32px;margin-bottom:12px">⏳</div><div id="bioSpinTitle" style="font-size:16px;font-weight:700;color:#0a1628;margin-bottom:6px"></div><div id="bioSpinMsg" style="font-size:13px;color:#6b7280"></div></div>';
    document.body.appendChild(s);
  }
  s.style.display = 'flex';
  document.getElementById('bioSpinTitle').textContent = title || 'Loading...';
  document.getElementById('bioSpinMsg').textContent = msg || '';
}
function hideSpinner() {
  const s = document.getElementById('bioSpinner');
  if (s) s.style.display = 'none';
}

// Mobile nav toggle
function toggleMobileNav() {
  const nl = document.getElementById('navLinks');
  if (nl) nl.classList.toggle('open');
}

// Nav search
function handleNavSearch(q) {
  if (!q.trim()) return;
  window.location.href = 'index.html#search-' + encodeURIComponent(q.trim());
}

// Active nav link
function setActiveNav(pageId) {
  document.querySelectorAll('.nav-link').forEach(a => a.classList.remove('active'));
  const el = document.getElementById('nl-' + pageId);
  if (el) el.classList.add('active');
}

// SEO meta updater
function updatePageSEO(title, desc, keywords, canonical) {
  if (title) document.title = title;
  const m = (id) => document.querySelector(id);
  if (desc) { const d = m('meta[name="description"]'); if(d) d.content = desc; }
  if (keywords) { const k = m('meta[name="keywords"]'); if(k) k.content = keywords; }
  if (canonical) { const c = m('link[rel="canonical"]'); if(c) c.href = canonical; }
}

// Apply Firebase-stored SEO for page
function applyFirebaseSEO(pageKey) {
  db.ref('/device_seo/' + pageKey).once('value').then(snap => {
    const s = snap.val();
    if (s) updatePageSEO(s.title, s.desc, s.keywords, s.url);
  });
}

// On load
document.addEventListener('DOMContentLoaded', function() {
  updateCartBadge();
});

// ── Hamburger mobile nav ──────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', function() {
  // Insert mobile menu if hamburger exists
  var hamburger = document.getElementById('navHamburger');
  if (!hamburger) return;
  hamburger.addEventListener('click', function() {
    hamburger.classList.toggle('open');
    var menu = document.getElementById('navMobileMenu');
    if (menu) menu.classList.toggle('open');
  });
  // Close on outside click
  document.addEventListener('click', function(e) {
    var menu = document.getElementById('navMobileMenu');
    if (!menu) return;
    if (!hamburger.contains(e.target) && !menu.contains(e.target)) {
      hamburger.classList.remove('open');
      menu.classList.remove('open');
    }
  });
});
