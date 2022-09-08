import requests
from os.path import dirname, realpath

test_dir = dirname(realpath(__file__))

# https 200 OK
r = requests.get('https://10.0.0.1/index.html', verify=False)
assert(r.status_code == 200)
assert(open(test_dir + '/../index.html', 'rb').read() == r.content)
print("Test 1 PASS!")

# http 301
r = requests.get('http://10.0.0.1/index.html', allow_redirects=False)
assert(r.status_code == 301 and r.headers['Location'] == 'https://10.0.0.1/index.html')
print("Test 2 PASS!")

# http 404
r = requests.get('http://10.0.0.1/notfound.html', verify=False)
assert(r.status_code == 404)
print("Test 3 PASS!")

# http 200 OK
r = requests.get('http://10.0.0.1/index.html', verify=False)
assert(r.status_code == 200 and open(test_dir + '/../index.html', 'rb').read() == r.content)
print("Test 4 PASS!")

# file in directory
r = requests.get('http://10.0.0.1/dir/index.html', verify=False)
assert(r.status_code == 200 and open(test_dir + '/../index.html', 'rb').read() == r.content)
print("Test 5 PASS!")

# http 206
headers = { 'Range': 'bytes=100-200' }
r = requests.get('http://10.0.0.1/index.html', headers=headers, verify=False)
assert(r.status_code == 206 and open(test_dir + '/../index.html', 'rb').read()[100:201] == r.content)
print("Test 6 PASS!")

# http 206
headers = { 'Range': 'bytes=100-' }
r = requests.get('http://10.0.0.1/index.html', headers=headers, verify=False)
print(r.content)
print()
print(open(test_dir + '/../index.html', 'rb').read()[100:])
print()
assert(r.status_code == 206 and open(test_dir + '/../index.html', 'rb').read()[100:] == r.content)
print("Test 7 PASS!")

print("RUN ALL 7 TESTS")
print("----PASS!")