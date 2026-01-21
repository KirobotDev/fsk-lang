module.exports = {
    apps: [{
        name: "fsk-server",
        script: "./fsk",
        args: "docs/server.fsk",
        interpreter: "none",
        cwd: ".",
        exec_mode: "fork",
        env: {
            NODE_ENV: "production",
        }
    }]
};