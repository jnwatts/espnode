# SSL certificate authority

These scripts should be run on a secure machine: If anyone obtains your root or intermediate keys then they can create unlimited TRUSTED certificates.

First run init_ca.sh, supplying personalized parameters. Next run init_server.sh with your MQTT server's FQDN: The CN for this certificate MUST match teh FQDN.

After initialization, the root CA key should be moved to a more secure (offline) location. You will only need it to update or re-issue the intermediate certificate.

# Credit

These commands were developed from https://jamielinux.com/docs/openssl-certificate-authority/index.html
