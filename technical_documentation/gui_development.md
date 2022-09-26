Amongst various other frontends, Munt has an electron based frontend that sits on top of our unified backend.
This makes contributing toward the user interface more accessible to a wider audience of developers.

To get started tinkering with the electron frontend and/or contributing changes, these are the required steps:
1. Install prerequisites yarn/npm/node/git
2. Check out a copy of the repository using `git`
3. Select an appropriate branch. For development purposes you should use the latest development branch
4. Change into `src/frontend/electron_vue` folder
5. Type `yarn` to install the depedencies
6. Type `yarn libunity:copy` to fetch the latest unified backend plugin
7. Type `yarn electron:serve` to launch the program

Notes:
* Development version runs in a different data directory (munt_dev) than your regular wallet
* You can't/shouldn't run both versions simultaneously unless you change the ports of one of them 

Troubleshooting:
* `error An unexpected error occurred: "https://npm.fontawesome.com/@fortawesome%2ffontawesome-pro: authentication required".
` Unfortunately currently it is not possible to run without a license for fontawesome pro, which the developers do have for releases but that we cannot share the key of with end users.
To add your own license if you have one
```
npm config set "@fortawesome:registry" "https://npm.fontawesome.com/"
npm config set "//npm.fontawesome.com/:_authToken=YOURAUTHTOKENGOESHERE"
```
We are still looking into ways to allow users to run without this dependency for development, pull requests welcome.
