module.exports = {
    apps: [
        {
            name: "fsk-docs",
            args: "server.fsk",
            script: "fsk",
            cwd: "./docs",
            interpreter: "none",
            exec_mode: "fork",
            env: {
                NODE_ENV: "production",
            }
        },
        {
            name: "fsk-registry",
            script: "server.js",
            cwd: "./registry",
            interpreter: "node",
            env: {
                PORT: 3000,
                NODE_ENV: "production"
            }
        }
    ]
};