## Google Root Certificates 

In order for the WINC TLS stack to connect to the google endpoint the root certs Google is using must be loaded into the module first since those roots are not included by default

These certs have extracted from the office source: [Google Root Certs](https://pki.goog/roots.pem)

Because the official list of root certs is far too large to be loaded entirely only a subset of them have been provided here. These have been shown to work generally but if the 
examples fail to connect adding the socket option "SO_SSL_BYPASS_X509_VERIF" will skip the certificate chain verification. If the example connects then it confirms the root certificate
is not loaded properly. Observing the diagnostic output of WINC module by connecting "UART DEBUG" to a uart will show which certificate failed to validate. See the 
[WINC1500 Xplained Pro User Guide](http://www.atmel.com/Images/Atmel-42388-ATWINC1500-Xplained-Pro_UserGuide.pdf) for more details on the uart diagnostic port.

These certificates will have to be copied to your WINC1500 firmware update project in the following path.

### <winc_firmware_update_base>\<winc_firmware_update_project>\src\firmware\Tools\root_certificate_downloader\binary

The firmware loader will likely require you to remove some existing certificates in order to make room. Which certificates you elect to keep will depend on your application.