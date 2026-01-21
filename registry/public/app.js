const API_URL = 'http://localhost:8059/api';
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
    container.innerHTML = '';

    if (list.length === 0) {
        container.innerHTML = '<p style="color: grey;">> No packages found.</p>';
        return;
    }

    list.forEach(pkg => {
        const date = new Date(pkg.created_at).toLocaleDateString();
        const html = `
            <div class="package-card">
                <h3>${pkg.name}</h3>
                <p>${pkg.description || 'No description provided.'}</p>
                <code class="package-url">fpm install ${pkg.name}</code>
                <br>
                <small style="color: #666;">By ${pkg.author || 'Anonymous'} | ${date}</small>
            </div>
        `;
        container.innerHTML += html;
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
