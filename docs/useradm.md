# User Database UI (useradm)

The `useradm` utility maintains a database of users.

Each user record has the following fields:

- Username
- Real Name
- Email Address
- Password Hash (SHA-1 hashed)
- Created Date
- Updated Date
- Last Seen Date
- Access Level (0-255)
- Locked (boolean)

The user database should have the following additional indexes:

- Username
- Email Address

Both usernames and email addresses should be unique. 
Two users should not have the same email address or username.
Usernames and email addresses are case insensitive (convert to lower case before using).

When an admin runs the application, a menu of options should be displayed.

1. Find a user by username
2. Find a user by email address
3. Find a user by ID
4. List all users
5. Create a new user
6. Exit

If the admin selects one of the `Find a user` options, the program should prompt the user for the value to
search by, lookup the user in the database using the proper index, and display the user. If the user can
not be found, then an error should be displayed.

If the admin selects `List all users` then a paginated table of users should be displayed, including ID, username, email, access level, and locked status.
At the end of each page of output the user should be prompted for an ID number. If a value is provided then that user should be looked up and displayed.
If no value is provided, the next page should be displayed. 

If the admin selects "Create a new user" then they should be prompted to enter each field of the user record except for the date fields.
After being created, the user should be displayed.

When a user is displayed on the screen, the admin should be prompted to edit, delete, or return to the menu.
If they choose delete the program should verify with the admin that they really want to delete the record and then delete the record
if asked to do so.
If they choose edit then the admin should ge given the choice of which field to edit and then asked for the new value.

The progam should not use any screen drawing libraries. Only the standard C library calls should be used, although ANSI escape characters may be used
to clear the screen, move the cursor, or control colors if needed. 

Only ASCII characters should be used.
