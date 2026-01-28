## Installing IOT utilities

From EdgeX version 3.0, the C utilities used by the SDK must be installed as a pre-requisite package,
rather than being downloaded and built with the SDK itself as in previous versions. Note that if re-using
an old build tree, the `src/c/iot` and `include/iot` directories must be removed as these will be
outdated.

All commands shown are to be run as the root user.

### Debian and Ubuntu

Management of package signing keys is changed in newer versions. For Debian 11 and Ubuntu 22.04:

```
apt-get install lsb-release apt-transport-https curl gnupg
curl -fsSL https://iotech.jfrog.io/artifactory/api/gpg/key/public | gpg --dearmor -o /usr/share/keyrings/iotech.gpg
echo "deb [signed-by=/usr/share/keyrings/iotech.gpg] https://iotech.jfrog.io/iotech/debian-release $(lsb_release -cs) main" | tee -a /etc/apt/sources.list.d/iotech.list
apt-get update
apt-get install iotech-iot-1.6-dev
```

For earlier versions:

```
apt-get install lsb-release apt-transport-https curl gnupg
curl -fsSL https://iotech.jfrog.io/artifactory/api/gpg/key/public | apt-key add -
echo "deb https://iotech.jfrog.io/iotech/debian-release $(lsb_release -cs) main" | tee -a /etc/apt/sources.list.d/iotech.list
apt-get update
apt-get install iotech-iot-1.6-dev
```

### Alpine

```
wget https://iotech.jfrog.io/artifactory/api/security/keypair/public/repositories/alpine-release -O /etc/apk/keys/alpine.dev.rsa.pub
echo "https://iotech.jfrog.io/artifactory/alpine-release/v3.16/main" >> /etc/apk/repositories
apk update
apk add iotech-iot-1.6-dev
```

Note: If not using Alpine 3.16, replace v3.16 in the above commands with the correct version.
