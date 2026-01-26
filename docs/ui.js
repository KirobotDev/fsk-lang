
(function () {
    function loadScript(src) {
        return new Promise((resolve, reject) => {
            const s = document.createElement('script');
            s.src = src;
            s.onload = resolve;
            s.onerror = reject;
            document.head.appendChild(s);
        });
    }

    const iframe = document.createElement('iframe');
    Object.assign(iframe.style, {
        width: '100vw', height: '100vh', border: 'none', position: 'absolute', top: '0', left: '0', background: '#222'
    });
    document.body.appendChild(iframe);

    const doc = iframe.contentDocument || iframe.contentWindow.document;
    doc.open();
    doc.write('<style>body{background:#111;color:#0f0;font-family:monospace;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}</style><h1>Initializing FSK Kernel...</h1>');
    doc.close();

    let isRequestPending = false;

    function sendRequest(path) {
        if (!window.FS) return;
        if (isRequestPending) return;

        console.log("[ROUTER] Navigating to:", path);
        isRequestPending = true;

        try {
            let method = "GET";
            if (path.startsWith("POST ")) {
            } else {
                path = "GET " + path + " HTTP/1.1";
            }

            FS.writeFile('request.tmp', path);
        } catch (e) {
            console.error("FS Write Error", e);
            isRequestPending = false;
        }
    }

    function injectInterceptors() {
        const win = iframe.contentWindow;
        const doc = win.document;

        doc.addEventListener('click', (e) => {
            let target = e.target;
            while (target && target.tagName !== 'A') target = target.parentElement;

            if (target && target.tagName === 'A') {
                const href = target.getAttribute('href');
                if (href && href.startsWith('/')) {
                    e.preventDefault();
                    history.pushState({}, "", href);
                    sendRequest(href);
                }
            } else if (e.target.tagName === 'BUTTON' && e.target.classList.contains('run-btn')) {
            }
        });

        win.fetch = async (url, options) => {
            if (url === '/api/run') {
                const body = options.body;
                const rawReq = "POST /api/run HTTP/1.1\r\n\r\n" + body;

                FS.writeFile('request.tmp', rawReq);

                return new Promise(resolve => {
                    const check = setInterval(() => {
                        try {
                            if (FS.analyzePath('response.tmp').exists) {
                                const res = FS.readFile('response.tmp', { encoding: 'utf8' });
                                FS.unlink('response.tmp');
                                clearInterval(check);

                                resolve({
                                    text: () => Promise.resolve(res)
                                });
                            }
                        } catch (e) { }
                    }, 50);
                });
            }
            return Promise.reject("Fetch intercepted");
        };
    }

    function startPolling() {
        setInterval(() => {
            if (!window.FS) return;

            try {
                const poll = FS.analyzePath('response.tmp');
                if (poll.exists) {
                    console.log("[ROUTER] Response received!");
                    const content = FS.readFile('response.tmp', { encoding: 'utf8' });
                    FS.unlink('response.tmp');

                    console.log("[ROUTER] Content Length:", content.length);
                    console.log("[ROUTER] Preview:", content.substring(0, 500));

                    if (content.length < 50) {
                        console.error("[ROUTER] Content too short! Potential error.");
                    }

                    iframe.srcdoc = content;

                    iframe.onload = () => {
                        injectInterceptors();
                        console.log("[ROUTER] Iframe Loaded.");
                    };

                    isRequestPending = false;
                }
            } catch (e) {
            }
        }, 100);
    }

    window.Module = {
        print: (t) => { console.log("[FSK]", t); },
        printErr: (t) => { console.error("[FSK ERR]", t); },
        onRuntimeInitialized: () => {
            console.log("Fsk Runtime Ready. Starting Server...");

            if (!window.FS && Module.FS) window.FS = Module.FS;

            Module.callMain(['web/main.fsk']);

            const path = window.location.pathname === '/index.html' ? '/' : window.location.pathname;
            sendRequest(path || '/');
            startPolling();
        }
    };

    window.onpopstate = () => {
        sendRequest(window.location.pathname);
    };

    loadScript('fsk.js');
})();
