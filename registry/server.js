const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = 3000;

app.use(cors());
app.use(express.json());
app.use(express.static('public'));

const db = new sqlite3.Database('./packages.db', (err) => {
    if (err) console.error('DB Error:', err.message);
    else console.log('Connected to SQLite database.');
});

db.serialize(() => {
    db.run(`CREATE TABLE IF NOT EXISTS packages (
        name TEXT PRIMARY KEY,
        url TEXT NOT NULL,
        description TEXT,
        author TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);
});


app.get('/api/packages', (req, res) => {
    const sql = "SELECT * FROM packages ORDER BY created_at DESC";
    db.all(sql, [], (err, rows) => {
        if (err) return res.status(500).json({ error: err.message });
        res.json({ data: rows });
    });
});

app.get('/api/packages/:name', (req, res) => {
    const sql = "SELECT * FROM packages WHERE name = ?";
    db.get(sql, [req.params.name], (err, row) => {
        if (err) return res.status(500).json({ error: err.message });
        if (!row) return res.status(404).json({ error: "Package not found" });
        res.json(row);
    });
});

app.post('/api/publish', (req, res) => {
    console.log("Headers:", req.headers);
    console.log("Body:", req.body);
    const { name, url, description, author } = req.body || {};
    if (!name || !url) {
        return res.status(400).json({ error: "Name and URL are required" });
    }

    const sql = "INSERT INTO packages (name, url, description, author) VALUES (?, ?, ?, ?)";
    db.run(sql, [name, url, description || "", author || "Anonymous"], function (err) {
        if (err) {
            if (err.message.includes("UNIQUE constraint failed")) {
                return res.status(409).json({ error: "Package name already taken" });
            }
            return res.status(500).json({ error: err.message });
        }
        res.json({ message: "Package published!", id: this.lastID });
    });
});

app.listen(PORT, () => {
    console.log(`Registry running on http://localhost:${PORT}`);
});
