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
        p.style.color = 'var(--c-text-tertiary)';
        p.style.textAlign = 'center';
        p.style.padding = '2rem';
        p.textContent = '> No packages found in the network.';
        container.appendChild(p);
        return;
    }

    list.forEach(pkg => {
        const date = new Date(pkg.created_at).toLocaleDateString();

        const card = document.createElement('div');
        card.className = 'package-card';

        const header = document.createElement('div');
        header.className = 'card-header';

        const titleGroup = document.createElement('div');
        titleGroup.style.display = 'flex';
        titleGroup.style.alignItems = 'center';
        titleGroup.style.gap = '10px';

        const h3 = document.createElement('h3');
        h3.textContent = pkg.name;
        h3.style.marginBottom = '0';

        const badge = document.createElement('span');
        badge.className = 'badge';
        badge.textContent = 'LATEST';

        titleGroup.append(h3, badge);

        const meta = document.createElement('div');
        meta.className = 'card-meta';
        meta.textContent = `By ${pkg.author || 'Anonymous'} • ${date}`;

        header.append(titleGroup, meta);

        const p = document.createElement('p');
        p.style.color = 'var(--c-text-secondary)';
        p.style.marginBottom = '1.5rem';
        p.textContent = pkg.description || 'No description provided for this package.';

        const footer = document.createElement('div');
        footer.style.background = 'var(--c-bg-sub)';
        footer.style.padding = '10px';
        footer.style.borderRadius = 'var(--radius-md)';
        footer.style.border = '1px solid var(--c-bg-border)';
        footer.style.fontFamily = 'var(--font-mono)';
        footer.style.fontSize = '0.9rem';
        footer.style.display = 'flex';
        footer.style.justifyContent = 'space-between';
        footer.style.alignItems = 'center';

        const cmd = document.createElement('span');
        cmd.textContent = `fpm install ${pkg.name}`;
        cmd.style.color = 'var(--c-brand)';
        cmd.style.fontWeight = 'bold';

        const copyBtn = document.createElement('button');
        copyBtn.className = 'btn btn-secondary';
        copyBtn.style.padding = '4px 8px';
        copyBtn.style.fontSize = '0.8rem';
        copyBtn.textContent = 'COPY';
        copyBtn.onclick = () => {
            navigator.clipboard.writeText(`fpm install ${pkg.name}`);
            copyBtn.textContent = 'COPIED!';
            setTimeout(() => copyBtn.textContent = 'COPY', 2000);
        };

        footer.append(cmd, copyBtn);

        card.append(header, p, footer);
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
