# Google Sheets Auth on Tines
These instructions enable you to read/write specific Google Sheets from Tines without requiring full access to all of your Sheets or requiring Delegated Domain Access. 

There are quite a few steps the first time you do this, almost all of them over on Google, but it's not difficult. Once it's done, you can re-use the same config over and over with each new Sheet with almost zero effort. The only change each time will be the Sheet id and the requirement to share the new sheet with the Service Account email address below.

## Google Service Account Setup

1. Go to the [Google Developers Console](https://console.cloud.google.com/apis/dashboard)
2. Select your project or create a new one (and then select it)
3. Enable the Sheets API for your project. In the sidebar on the left, select APIs & Services > Library
    1. Search for "Sheets"
    2. Click on "Google Sheets API"
    3. click the blue "Enable" button
4. In the sidebar on the left, select APIs & Services > Credentials
5. Click blue "+ CREATE CREDENITALS" and select "Service account" option
6. Enter name, description, click "CREATE"
7. You can skip permissions, click "CONTINUE"
8. Click "+ CREATE KEY" button
9. Select the "JSON" key type option
10. Click "Create" button
11. Your JSON key file is generated and downloaded to your machine (it is the only copy so keep it safe!)
12. Click "DONE"
13. Note your service account's email address (also available in the JSON key file)
14. **Share the Google Sheet(s) you want to access with your service account using the email noted above** - This is the step you are probably going to forget in the future. Try not to :-)

## Tines Setup
The first time you do this requires several steps, but once again, for each new Story after that, it's incredibly easy.

### First Part: The JWT 
1. Sign into your Tines Account and select Credentials -> New
2. Under “Type” chose “JWT”
3. Enter a credential name like `Google Sheets JWT`
4. Under “Algorithm” chose RSA256.
5. Define the payload for our JWTas follows:

```json
{ 
  "iss":"the_service_account_email",   
  "scope":"https://www.googleapis.com/auth/spreadsheets",   
  "aud":"https://www.googleapis.com/oauth2/v4/token"   
}
```

**NOTE:** Delete the *Sub* field that is inserted by default unless you want Domain Wide Delegation and you have the permissions to do that in your Google Account.

**NOTE 2:** The Scope above is set to give read/write access to the Google Sheets that you share with the Service Account email.  

6. Select the “ Auto generate ‘iat’ (Issued At) & ‘exp’ (Expiration Time) claims” checkbox. so that Tines will add “iat” and “exp” claims to the payload according to when the credential is used.
7. Copy and paste the private key from the Google private key JSON file you downloaded earlier.
8. Click “Save Credential”
9. You can now refer to that JWT via *{{.CREDENTIAL.google_sheets_jwt}}*

### Second Part: The Access Token

Before we can call the Google APIs, we need an access token. Tines has a very neat way of doing this so that you get a new token each time the story is run (otherwise these tokens can expire quite quickly).

1. Go to Credentials again
2. Create new Credential
3. Give it the name like `GSheets Bearer`
4. Set the Type to "HTTP Request"
5. In the Plain Code Builder, paste this in:

```json
{
  "url": "https://www.googleapis.com/oauth2/v4/token",
  "content_type": "form",
  "method": "post",
  "payload": {
    "grant_type": "urn:ietf:params:oauth:grant-type:jwt-bearer",
    "assertion": "{{.CREDENTIAL.google_sheets_jwt }}"
  },
  "expected_update_period_in_days": "1"
}
```

1. In "Location of Token from Response" use `{{ .gsheets_bearer.body.access_token }}`
2. Click "Save Credential"
3. Every time you access that Credential in your story using `{{.CREDENTIAL.gsheets_bearer}}`, Tines will actually make the HTTP Request above and refresh the Token for you.

### Third Part: Using the Access Token

1. In your Story you may be writing to Google Sheets using the following type of URL:

```sh
https://sheets.googleapis.com/v4/spreadsheets/{{.RESOURCE.gsheets_lowcode_sheet_id}}/values/Sheet1!A1:Z999:append?valueInputOption=USER_ENTERED
```

2. This might have a Payload something like this:

```json
{
  "values": [
    [
      "{{.explode_array_of_incoming_alerts.individual_record.published }}",
      "{{.explode_array_of_incoming_alerts.individual_record.title }}",
      "{{.get_original_story_url.urls.first }}",
      "{{.explode_array_of_incoming_alerts.individual_record.content }}",
      "{{.explode_array_of_incoming_alerts.individual_record.id }}"
    ]
  ]
}
```

3. In order to access it, you provide it with the latest Token by sending an Authorization Header like so:

```bash
Bearer {{.CREDENTIAL.gsheets_bearer}}
```

4. You should never have to worry about your Tokens expiring
5. One note of caution is to think about how often you are making the HTTP Request e.g. for every element of an exploded array may be too frequent. It may be prudent to refresh the token at the top of a story and then make it available elsewhere in the story as a new value. You can do this by inserting a *message_only* Event Transformation Action called perhaps `Save Refresh Token For Later``, where the Payload is something like:

```json
{
  "gsheets_refresh_token": "{{.CREDENTIAL.gsheets_bearer }}"
}
```

6. Then you would use `.save_refresh_token_for_later.gsheets_refresh_token` elsewhere in the Story.
