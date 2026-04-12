// ═══ FIREBASE CONFIG — Shared across all Biomedionics pages ═══
const FB_CONFIG = {
  apiKey:            "AIzaSyBJ0jSR1nyRtZmOrsYTd0fxT0jvVxUudm8",
  authDomain:        "biomedionics-5d664.firebaseapp.com",
  databaseURL:       "https://biomedionics-5d664-default-rtdb.firebaseio.com",
  projectId:         "biomedionics-5d664",
  storageBucket:     "biomedionics-5d664.firebasestorage.app",
  messagingSenderId: "59681602646",
  appId:             "1:59681602646:web:4fe07cdef3f06173460ff7"
};
// Safe init — works with firebase-compat v9
try {
  firebase.app(); // throws if no app initialized
} catch(e) {
  firebase.initializeApp(FB_CONFIG);
}
const db      = firebase.database();
const storage = firebase.storage();
