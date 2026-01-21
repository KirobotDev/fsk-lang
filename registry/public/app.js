const API_URL = 'https://fpm.kirosb.fr/api';
let allPackages = [];

async function fetchPackages() {
    try {
        const res = await fetch(`${API_URL}/packages`);
        const json = await res.json();
        allPackages = json.data;
        renderPackages(allPackages);
    } catch (e) {
        console.error(e);
    }
}

function renderPackages(list) {
    const container = document.getElementById('packageList');
    container.replaceChildren();

    if (list.length === 0) {
        const p = document.createElement('p');
        p.style.color = 'grey';
        p.textContent = '> No packages found.';
        container.appendChild(p);
        return;
    }

    list.forEach(pkg => {
        const date = new Date(pkg.created_at).toLocaleDateString();

        const card = document.createElement('div');
        card.className = 'package-card';

        const h3 = document.createElement('h3');
        h3.textContent = pkg.name;

        const p = document.createElement('p');
        p.textContent = pkg.description || 'No description provided.';

        const code = document.createElement('code');
        code.className = 'package-url';
        code.textContent = `fpm install ${pkg.name}`;

        const br = document.createElement('br');

        const small = document.createElement('small');
        small.style.color = '#666';
        small.textContent = `By ${pkg.author || 'Anonymous'} | ${date}`;

        card.append(h3, p, code, br, small);
        container.appendChild(card);
    });
}

function filterPackages() {
    const query = document.getElementById('search').value.toLowerCase();
    const filtered = allPackages.filter(p =>
        p.name.toLowerCase().includes(query) ||
        (p.description && p.description.toLowerCase().includes(query))
    );
    renderPackages(filtered);
}

document.getElementById('publishForm').addEventListener('submit', async (e) => {
    e.preventDefault();

    const data = {
        name: document.getElementById('pName').value,
        url: document.getElementById('pUrl').value,
        description: document.getElementById('pDesc').value,
        author: document.getElementById('pAuthor').value
    };

    try {
        const res = await fetch(`${API_URL}/publish`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });

        const json = await res.json();

        if (res.ok) {
            alert('SUCCESS: Package published to the network.');
            document.getElementById('publishForm').reset();
            fetchPackages();
        } else {
            alert('ERROR: ' + json.error);
        }
    } catch (e) {
        alert('NETWORK ERROR');
    }
});

fetchPackages();
