
(function() {
    function loadScript(src) {
        return new Promise((resolve, reject) => {
            const s = document.createElement('script');
            s.src = src;
            s.onload = resolve;
            s.onerror = reject;
            document.head.appendChild(s);
        });
    }

    function create(tag, style, parent) {
        const el = document.createElement(tag);
        if (style) Object.assign(el.style, style);
        if (parent) parent.appendChild(el);
        return el;
    }

    async function init() {
        await loadScript('https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js');
        await loadScript('https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js');

        const container = create('div', {width:'100vw', height:'100vh', position:'absolute', top:'0', left:'0', zIndex:'1'}, document.body);
        container.id = 'canvas-container';

        const uiLayer = create('div', {
            position:'absolute', top:'20px', right:'20px', zIndex:'100', 
            fontFamily:'monospace', display:'flex', flexDirection:'column', alignItems:'flex-end', pointerEvents:'none'
        }, document.body);

        const wrapper = create('div', {
            width:'450px', height:'300px', backgroundColor:'rgba(10,10,10,0.9)', 
            border:'1px solid #333', borderRadius:'4px', overflow:'hidden', 
            pointerEvents:'auto', display:'flex', flexDirection:'column'
        }, uiLayer);
        
        const header = create('div', {
            background:'#111', padding:'5px 10px', borderBottom:'1px solid #333',
            color:'#666', fontSize:'10px', display:'flex', justifyContent:'space-between'
        }, wrapper);
        header.textContent = 'FSK TERMINAL';

        const consoleDiv = create('div', {
            flex:'1', padding:'10px', color:'#0f0', fontSize:'12px', overflowY:'auto', fontFamily:'monospace'
        }, wrapper);
        consoleDiv.id = 'console';

        const log = (msg, color='#0f0') => {
            const line = create('div', {marginBottom:'4px', color:color}, consoleDiv);
            const ts = create('span', {color:'#444', marginRight:'8px', fontSize:'10px'}, line);
            ts.textContent = new Date().toLocaleTimeString('en-US',{hour12:false});
            const txt = create('span', {}, line);
            txt.textContent = '>> ' + msg;
            consoleDiv.scrollTop = consoleDiv.scrollHeight;
        };
        
        window.Module = {
            print: (t) => { if(t) { console.log(t); log(t); } },
            printErr: (t) => { if(t) { console.error(t); log(t, '#f33'); } },
            onRuntimeInitialized: () => { log('System Ready.', '#fff'); Module.callMain(['web/main.fsk']); }
        };

        const scene = new THREE.Scene();
        scene.background = new THREE.Color(0x000000);
        const camera = new THREE.PerspectiveCamera(45, window.innerWidth/window.innerHeight, 0.1, 1000);
        camera.position.set(0,2,5);
        
        const renderer = new THREE.WebGLRenderer({antialias:true});
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        container.appendChild(renderer.domElement);
        
        const controls = new THREE.OrbitControls(camera, renderer.domElement);
        controls.enableDamping = true;
        controls.minDistance = 2.5;

        const light = new THREE.DirectionalLight(0xffffff, 1.2);
        light.position.set(50,20,30);
        scene.add(light);
        scene.add(new THREE.AmbientLight(0x404040, 0.2));

        const starsGeo = new THREE.BufferGeometry();
        const pos = [];
        for(let i=0;i<9000;i++) pos.push((Math.random()-0.5)*200);
        starsGeo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
        const stars = new THREE.Points(starsGeo, new THREE.PointsMaterial({color:0xffffff, size:0.1, transparent:true}));
        scene.add(stars);

        const grp = new THREE.Group();
        scene.add(grp);
        const l = new THREE.TextureLoader();
        const earth = new THREE.Mesh(
            new THREE.SphereGeometry(1,64,64),
            new THREE.MeshPhongMaterial({
                map: l.load('https://unpkg.com/three-globe/example/img/earth-blue-marble.jpg'),
                specularMap: l.load('https://unpkg.com/three-globe/example/img/earth-water.png'),
                bumpMap: l.load('https://unpkg.com/three-globe/example/img/earth-topology.png'),
                specular: new THREE.Color('grey')
            })
        );
        grp.add(earth);
        
        const clouds = new THREE.Mesh(
            new THREE.SphereGeometry(1.01,64,64),
            new THREE.MeshPhongMaterial({
                map: l.load('https://unpkg.com/three-globe/example/img/earth-clouds.png'),
                transparent:true, opacity:0.8, side:THREE.DoubleSide, blending:THREE.AdditiveBlending
            })
        );
        grp.add(clouds);

        const animate = () => {
            requestAnimationFrame(animate);
            clouds.rotation.y += 0.0002;
            earth.rotation.y += 0.00005;
            controls.update();
            renderer.render(scene, camera);
        };
        animate();
        
        window.onresize = () => {
            camera.aspect = window.innerWidth/window.innerHeight;
            camera.updateProjectionMatrix();
            renderer.setSize(window.innerWidth, window.innerHeight);
        };

        loadScript('fsk.js');
    }
    init();
})();
