# ezRemote Client

ezRemote Client is a File Manager application that allows you to connect the VITA to remote FTP, SMB, NFS, WebDAV, HTTP servers to transfer and manage files. The interface is inspired by Filezilla client which provides a commander like GUI.

![Preview](/screenshot.jpg)

## Features
 - Transfer files back and forth between VITA and FTP/SMB/NFS/WebDAV/Http(Rclone,IIS,nginx,apache,NpxServe) server
 - File management function include cut/copy/paste/rename/delete/new folder/file for files on VITA local drives
 - Install homebrew packages in VPK,ZIP,RAR,7ZIP/Folders from both Local and Remote
 - Install NPS_Browser generates packages in ZIP,RAR,7ZIP/Folders from both Local and Remote
 - Extract ZIP, RAR, 7ZIP files for both Local and Remote
 - Create ZIP package on Local

## Usage
To distinguish between FTP, SMB, NFS, WebDAV or HTTP, the URL must be prefix with **ftp://**, **smb://**, **nfs://**, **webdav://**, **webdavs://**, **http://**, **https://**

 - The url format for FTP is
   ```
   ftp://hostname[:port]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 21(ftp) if not provided
   ```

 - The url format for SMB is
   ```
   smb://hostname[:port]/sharename

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 445 if not provided
     - sharename is required
   ```

 - The url format for NFS is
   ```
   nfs://hostname[:port]/export_path[?uid=<UID>&gid=<GID>]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 2049 if not provided
     - export_path is required
     - uid is the UID value to use when talking to the server. Defaults to 65534 if not specified.
     - gid is the GID value to use when talking to the server. Defaults to 65534 if not specified.

     Special characters in 'path' are escaped using %-hex-hex syntax.

     For example '?' must be escaped if it occurs in a path as '?' is also used to
     separate the path from the optional list of url arguments.

     Example:
     nfs://192.168.0.1/my?path?uid=1000&gid=1000
     must be escaped as
     nfs://192.168.0.1/my%3Fpath?uid=1000&gid=1000
   ```

 - The url format for WebDAV is
   ```
   webdav://hostname[:port]/[url_path]
   webdavs://hostname[:port]/[url_path]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 80(webdav) and 443(webdavs) if not provided
     - url_path is optional based on your WebDAV hosting requiremets
   ```
  
 - The url format for HTTP is
   ```
   http://hostname[:port]/[url_path]
   https://hostname[:port]/[url_path]

     - hostname can be the textual hostname or an IP address. hostname is required
     - port is optional and defaults to 80(http) and 443(https) if not provided
     - url_path is optional
   ```

- For Internet Archive repos download URLs
  - Only supports parsing of the download URL (ie the URL where you see a list of files). Example
    |      |           |  |
    |----------|-----------|---|
    | ![archive_org_screen1](https://github.com/user-attachments/assets/b129b6cf-b938-4d7c-a3fa-61e1c633c1f6) | ![archive_org_screen2](https://github.com/user-attachments/assets/646106d1-e60b-4b35-b153-3475182df968)| ![image](https://github.com/user-attachments/assets/cad94de8-a694-4ef5-92a8-b87468e30adb) |

- For Myrient repos, entry **https://myrient.erista.me/files** in the server field.
  ![image](https://github.com/user-attachments/assets/b80e2bec-b8cc-4acc-9ab6-7b0dc4ef20e6)

- Support for browse and download  release artifacts from github repos. Under the server just enter the git repo of the homebrew eg https://github.com/cy33hc/ps4-ezremote-client
  ![image](https://github.com/user-attachments/assets/f8e931ea-f1d1-4af8-aed5-b0dfe661a230)

Tested with following WebDAV server:
 - **(Recommeded)** [RClone](https://rclone.org/) - For hosting your own WebDAV server. You can use RClone WebDAV server as proxy to 70+ public file hosting services (Eg. Google Drive, OneDrive, Mega, dropbox, NextCloud etc..)
 - [Dufs](https://github.com/sigoden/dufs) - For hosting your own WebDAV server.
 - [SFTPgo](https://github.com/drakkan/sftpgo) - For local hosted WebDAV server. Can also be used as a webdav frontend for Cloud Storage like AWS S3, Azure Blob or Google Storage.

## Controls
```
Triangle - Menu (after a file(s)/folder(s) is selected)
Cross - Select Button/TextBox
Circle - Un-Select the file list to navigate to other widgets or Close Dialog window in most cases
Square - Mark file(s)/folder(s) for Delete/Rename/Upload/Download
R1 - Navigate to the Remote list of files
L1 - Navigate to the Local list of files
```

## Multi Language Support
The appplication support following languages.

The following languages are auto detected.
```
Dutch
English
French
German
Italiano
Japanese
Korean
Polish
Portuguese_BR
Russian
Spanish
Chinese_Simplified
Chinese_Traditional
```

The following aren't standard languages supported by the VITA, therefore requires a config file update.
```
Arabic
Catalan
Croatian
Euskera
Galego
Greek
Hungarian
Indonesian
Romanian
Ryukyuan
Thai
Turkish
```
User must modify the file **ux0:/data/RMTCLI001/config.ini** and update the **language** setting with the **exact** values from the list above.

**HELP:** There are no language translations for the following languages, therefore not support yet. Please help expand the list by submitting translation for the following languages. If you would like to help, please download this [Template](https://github.com/cy33hc/vita-ezremote-client/blob/master/lang/English.ini), make your changes and submit an issue with the file attached.
```
Finnish
Swedish
Danish
Norwegian
Czech
Vietnamese
```
or any other language that you have a traslation for.
