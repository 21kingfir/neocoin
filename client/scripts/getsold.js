async function getrsakey(ip) {
    try {
        const response = await fetch(ip);
        if (!response.ok) {
            throw new Error(response.status);
        }
        const data = await response.json();
        return data;
    } catch (error) {
        return;
    }
}

async function fetchgetaccountid(identifient, pwd, ipsite) {
    try {
        const donnee = await fetch(ip);

        if (!donnee.ok) {
            
        }
        
    }
}

const pemHeader = "-----BEGIN PUBLIC KEY-----\n";
const pemFooter = "-----END PUBLIC KEY-----\n";

const ipgetrsakey = "http://:8080/getrsakey";
const ipgetaccountid = "http://8080/getaccountid"

const id = localStorage.getItem("id");
const password = localStorage.getItem("password");

const rawdata = {"id":id, "password":password};
const jdata = JSON.stringify(rawdata);

const jsondata = getrsakey(ipgetrsakey);

const publickey = jsondata[0].text();
const idkey = jsondata[1];

const publickeypemwh = publickey.replace(pemHeader, "").replace(pemFooter, "");

const derpubkey = Uint8Array.from(atob(publickeypemwh), c => c.charCodeAt(0));

const finalpublickey = await crypto.subtle.importKey("spki", derpubkey.buffer, {name: "RSA-OAEP", hash:"SHA-256"}, false, ["encrypt"]);

const encoder = new TextEncoder();

const bytedid = encoder.encode(message);
const bytedpassword = encoder.encode(password);

const encid = await crypto.subtle.encrypt({ name: "RSA-OAEP"}, finalpublickey, bytedid);
const encpwd = await crypto.subtle.encrypt({ name: "RSA-OAEP"}, finalpublickey, bytedpassword);

const b64encid = btoa(String.fromCharCode(...new Uint8Array(encid)));
const b64encpwd = btoa(String.fromCharCode(...new Uint8Array(encpwd)));

