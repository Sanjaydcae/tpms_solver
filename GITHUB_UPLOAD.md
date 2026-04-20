# Upload TPMS Studio To GitHub

This machine currently needs Git/GitHub tooling before the project can be pushed.

## 1. Install Git And GitHub CLI

```bash
sudo apt-get update
sudo apt-get install -y git gh
```

## 2. Login To GitHub

```bash
gh auth login
```

Choose:

- GitHub.com
- HTTPS
- Login with browser

## 3. Create A New GitHub Repository

Option A: create it from the GitHub website.

Suggested repository name:

```text
tpms-studio
```

Because this is proprietary software, create it as a **private** repository unless H2one Cleantech Private Limited approves public release.

Option B: create it from the terminal after login:

```bash
cd /home/sanjay/Desktop/tpms_solver
gh repo create tpms-studio --private --source=. --remote=origin
```

## 4. Initialize And Push

If you created the repository on GitHub website:

```bash
cd /home/sanjay/Desktop/tpms_solver
git init
git add .gitignore README.md LICENSE.md GITHUB_UPLOAD.md CMakeLists.txt build.sh install_deps.sh src
git commit -m "Initial TPMS Studio MVP"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME_OR_ORG/tpms-studio.git
git push -u origin main
```

Replace:

```text
YOUR_USERNAME_OR_ORG
```

with your GitHub username or organization.

## 5. Verify GitHub Does Not Include Build Files

Run:

```bash
git status --ignored
```

The `build/` folder should be ignored and should not be uploaded.

## 6. Build After Clone

On another machine:

```bash
git clone https://github.com/YOUR_USERNAME_OR_ORG/tpms-studio.git
cd tpms-studio
bash build.sh
./build/tpms_solver
```
