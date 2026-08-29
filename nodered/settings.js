const bcrypt = require("/usr/src/node-red/node_modules/bcryptjs");

module.exports = {
    adminAuth: {
        type: "credentials",
        users: [{
            username: process.env.NODERED_ADMIN_USERNAME,
            password: bcrypt.hashSync(process.env.NODERED_ADMIN_PASSWORD, 8),
            permissions: "*"
        }]
    }
};
