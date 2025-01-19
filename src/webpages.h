////////////////////////////////////////////////////////////////////
/// @file webpages.h
/// @brief Contains all functions related to the webserver
////////////////////////////////////////////////////////////////////

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%APP_NAME%</title>
  <link rel="stylesheet" href="/simple.css">
</head>
<body id="top">
  <header>
    <h1>%APP_NAME% Admin page</h1>
  </header>
  <p>Firmware: %FIRMWARE%</p>
  <p>Free Storage: <span id="freespiffs">%FREESPIFFS%</span> | Used Storage: <span id="usedspiffs">%USEDSPIFFS%</span> | Total Storage: <span id="totalspiffs">%TOTALSPIFFS%</span></p>
  <p>
  <button onclick="logoutButton()">Logout</button>
  <button onclick="rebootButton()">Reboot</button>
  <button onclick="listFilesButton()">List Files</button>
  <button onclick="listSDFilesButton()">List SD Files</button>
  <button onclick="showUploadButtonFancy()">Upload File</button>
  </p>
  <p id="status"></p>
  <p id="detailsheader"></p>
  <p id="details"></p>
<script>
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
function rebootButton() {
  document.getElementById("status").innerHTML = "Invoking Reboot ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/reboot", true);
  xhr.send();
  window.open("/reboot","_self");
}
function listFilesButton() {
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listfiles", false);
  xmlhttp.send();
  document.getElementById("detailsheader").innerHTML = "<h3>SPIFFS Files<h3>";
  document.getElementById("details").innerHTML = xmlhttp.responseText;
}
function listSDFilesButton() {
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listSDfiles", false);
  xmlhttp.send();
  document.getElementById("detailsheader").innerHTML = "<h3>SD Files<h3>";
  document.getElementById("details").innerHTML = xmlhttp.responseText;
}
function downloadDeleteButton(filename, action) {
  var urltocall = "/file?name=" + filename + "&action=" + action;
  xmlhttp=new XMLHttpRequest();
  if (action == "delete") {
    xmlhttp.open("GET", urltocall, false);
    xmlhttp.send();
    document.getElementById("status").innerHTML = xmlhttp.responseText;
    xmlhttp.open("GET", "/listfiles", false);
    xmlhttp.send();
    document.getElementById("details").innerHTML = xmlhttp.responseText;
  }
  if (action == "download") {
    document.getElementById("status").innerHTML = "";
    window.open(urltocall,"_blank");
  }
}
function showUploadButtonFancy() {
  document.getElementById("detailsheader").innerHTML = "<h3>Upload File<h3>"
  document.getElementById("status").innerHTML = "";
  var uploadform = "<form method = \"POST\" action = \"/\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"data\"/><input type=\"submit\" name=\"upload\" value=\"Upload\" title = \"Upload File\"></form>"
  document.getElementById("details").innerHTML = uploadform;
  var uploadform =
  "<form id=\"upload_form\" enctype=\"multipart/form-data\" method=\"post\">" +
  "<input type=\"file\" name=\"file1\" id=\"file1\" onchange=\"uploadFile()\"><br>" +
  "<progress id=\"progressBar\" value=\"0\" max=\"100\" style=\"width:300px;\"></progress>" +
  "<h3 id=\"status\"></h3>" +
  "<p id=\"loaded_n_total\"></p>" +
  "</form>";
  document.getElementById("details").innerHTML = uploadform;
}
function _(el) {
  return document.getElementById(el);
}
function uploadFile() {
  var file = _("file1").files[0];
  // alert(file.name+" | "+file.size+" | "+file.type);
  var formdata = new FormData();
  formdata.append("file1", file);
  var ajax = new XMLHttpRequest();
  ajax.upload.addEventListener("progress", progressHandler, false);
  ajax.addEventListener("load", completeHandler, false); // doesnt appear to ever get called even upon success
  ajax.addEventListener("error", errorHandler, false);
  ajax.addEventListener("abort", abortHandler, false);
  ajax.open("POST", "/");
  ajax.send(formdata);
}
function progressHandler(event) {
  //_("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total; // event.total doesnt show accurate total file size
  _("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes";
  var percent = (event.loaded / event.total) * 100;
  _("progressBar").value = Math.round(percent);
  _("status").innerHTML = Math.round(percent) + "% uploaded... please wait";
  if (percent >= 100) {
    _("status").innerHTML = "Please wait, writing file to filesystem";
  }
}
function completeHandler(event) {
  _("status").innerHTML = "Upload Complete";
  _("progressBar").value = 0;
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listfiles", false);
  xmlhttp.send();
  document.getElementById("status").innerHTML = "File Uploaded";
  document.getElementById("detailsheader").innerHTML = "<h3>Files<h3>";
  document.getElementById("details").innerHTML = xmlhttp.responseText;
}
function errorHandler(event) {
  _("status").innerHTML = "Upload Failed";
}
function abortHandler(event) {
  _("status").innerHTML = "inUpload Aborted";
}
</script>
</body>
</html>
)rawliteral";

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <p><a href="/">Log Back In</a></p>
</body>
</html>
)rawliteral";

// reboot.html base upon https://gist.github.com/Joel-James/62d98e8cb3a1b6b05102
const char reboot_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
</head>
<body>
<h3>
  Rebooting, returning to main page in <span id="countdown">30</span> seconds
</h3>
<script type="text/javascript">
  var seconds = 20;
  function countdown() {
    seconds = seconds - 1;
    if (seconds < 0) {
      window.location = "/";
    } else {
      document.getElementById("countdown").innerHTML = seconds;
      window.setTimeout("countdown()", 1000);
    }
  }
  countdown();
</script>
</body>
</html>
)rawliteral";

const char simple_css[] PROGMEM = R"rawliteral(
:root{--sans-font:-apple-system,BlinkMacSystemFont,"Avenir Next",Avenir,"Nimbus Sans L",Roboto,"Noto Sans","Segoe UI",Arial,Helvetica,"Helvetica Neue",sans-serif;--mono-font:Consolas,Menlo,Monaco,"Andale Mono","Ubuntu Mono",monospace;--standard-border-radius:5px;--bg:#fff;--accent-bg:#f5f7ff;--text:#212121;--text-light:#585858;--border:#898ea4;--accent:#0d47a1;--accent-hover:#1266e2;--accent-text:var(--bg);--code:#d81b60;--preformatted:#444;--marked:#fd3;--disabled:#efefef}@media (prefers-color-scheme:dark){:root{color-scheme:dark;--bg:#212121;--accent-bg:#2b2b2b;--text:#dcdcdc;--text-light:#ababab;--accent:#ffb300;--accent-hover:#ffe099;--accent-text:var(--bg);--code:#f06292;--preformatted:#ccc;--disabled:#111}img,video{opacity:.8}}*,:before,:after{box-sizing:border-box}textarea,select,input,progress{-webkit-appearance:none;-moz-appearance:none;appearance:none}html{font-family:var(--sans-font);scroll-behavior:smooth}body{color:var(--text);background-color:var(--bg);grid-template-columns:1fr min(45rem,90%) 1fr;margin:0;font-size:1.15rem;line-height:1.5;display:grid}body>*{grid-column:2}body>header{background-color:var(--accent-bg);border-bottom:1px solid var(--border);text-align:center;grid-column:1/-1;padding:0 .5rem 2rem}body>header>:only-child{margin-block-start:2rem}body>header h1{max-width:1200px;margin:1rem auto}body>header p{max-width:40rem;margin:1rem auto}main{padding-top:1.5rem}body>footer{color:var(--text-light);text-align:center;border-top:1px solid var(--border);margin-top:4rem;padding:2rem 1rem 1.5rem;font-size:.9rem}h1{font-size:3rem}h2{margin-top:3rem;font-size:2.6rem}h3{margin-top:3rem;font-size:2rem}h4{font-size:1.44rem}h5{font-size:1.15rem}h6{font-size:.96rem}p{margin:1.5rem 0}p,h1,h2,h3,h4,h5,h6{overflow-wrap:break-word}h1,h2,h3{line-height:1.1}@media only screen and (width<=720px){h1{font-size:2.5rem}h2{font-size:2.1rem}h3{font-size:1.75rem}h4{font-size:1.25rem}}a,a:visited{color:var(--accent)}a:hover{text-decoration:none}button,.button,a.button,input[type=submit],input[type=reset],input[type=button]{border:1px solid var(--accent);background-color:var(--accent);color:var(--accent-text);padding:.5rem .9rem;line-height:normal;text-decoration:none}.button[aria-disabled=true],input:disabled,textarea:disabled,select:disabled,button[disabled]{cursor:not-allowed;background-color:var(--disabled);border-color:var(--disabled);color:var(--text-light)}input[type=range]{padding:0}abbr[title]{cursor:help;text-decoration-line:underline;text-decoration-style:dotted}button:enabled:hover,.button:not([aria-disabled=true]):hover,input[type=submit]:enabled:hover,input[type=reset]:enabled:hover,input[type=button]:enabled:hover{background-color:var(--accent-hover);border-color:var(--accent-hover);cursor:pointer}.button:focus-visible,button:focus-visible:where(:enabled),input:enabled:focus-visible:where([type=submit],[type=reset],[type=button]){outline:2px solid var(--accent);outline-offset:1px}header>nav{padding:1rem 0 0;font-size:1rem;line-height:2}header>nav ul,header>nav ol{flex-flow:wrap;place-content:space-around center;align-items:center;margin:0;padding:0;list-style-type:none;display:flex}header>nav ul li,header>nav ol li{display:inline-block}header>nav a,header>nav a:visited{border:1px solid var(--border);border-radius:var(--standard-border-radius);color:var(--text);margin:0 .5rem 1rem;padding:.1rem 1rem;text-decoration:none;display:inline-block}header>nav a:hover,header>nav a.current,header>nav a[aria-current=page],header>nav a[aria-current=true]{border-color:var(--accent);color:var(--accent);cursor:pointer}@media only screen and (width<=720px){header>nav a{border:none;padding:0;line-height:1;text-decoration:underline}}aside,details,pre,progress{background-color:var(--accent-bg);border:1px solid var(--border);border-radius:var(--standard-border-radius);margin-bottom:1rem}aside{float:right;width:30%;margin-inline-start:15px;padding:0 15px;font-size:1rem}[dir=rtl] aside{float:left}@media only screen and (width<=720px){aside{float:none;width:100%;margin-inline-start:0}}article,fieldset,dialog{border:1px solid var(--border);border-radius:var(--standard-border-radius);margin-bottom:1rem;padding:1rem}article h2:first-child,section h2:first-child,article h3:first-child,section h3:first-child{margin-top:1rem}section{border-top:1px solid var(--border);border-bottom:1px solid var(--border);margin:3rem 0;padding:2rem 1rem}section+section,section:first-child{border-top:0;padding-top:0}section+section{margin-top:0}section:last-child{border-bottom:0;padding-bottom:0}details{padding:.7rem 1rem}summary{cursor:pointer;word-break:break-all;margin:-.7rem -1rem;padding:.7rem 1rem;font-weight:700}details[open]>summary+*{margin-top:0}details[open]>summary{margin-bottom:.5rem}details[open]>:last-child{margin-bottom:0}table{border-collapse:collapse;margin:1.5rem 0}figure>table{width:max-content;margin:0}td,th{border:1px solid var(--border);text-align:start;padding:.5rem}th{background-color:var(--accent-bg);font-weight:700}tr:nth-child(2n){background-color:var(--accent-bg)}table caption{margin-bottom:.5rem;font-weight:700}textarea,select,input,button,.button{font-size:inherit;border-radius:var(--standard-border-radius);box-shadow:none;max-width:100%;margin-bottom:.5rem;padding:.5rem;font-family:inherit;display:inline-block}textarea,select,input{color:var(--text);background-color:var(--bg);border:1px solid var(--border)}label{display:block}textarea:not([cols]){width:100%}select:not([multiple]){background-image:linear-gradient(45deg,transparent 49%,var(--text)51%),linear-gradient(135deg,var(--text)51%,transparent 49%);background-position:calc(100% - 15px),calc(100% - 10px);background-repeat:no-repeat;background-size:5px 5px,5px 5px;padding-inline-end:25px}[dir=rtl] select:not([multiple]){background-position:10px,15px}input[type=checkbox],input[type=radio]{vertical-align:middle;width:min-content;position:relative}input[type=checkbox]+label,input[type=radio]+label{display:inline-block}input[type=radio]{border-radius:100%}input[type=checkbox]:checked,input[type=radio]:checked{background-color:var(--accent)}input[type=checkbox]:checked:after{content:" ";border-right:solid var(--bg).08em;border-bottom:solid var(--bg).08em;background-color:#0000;border-radius:0;width:.18em;height:.32em;font-size:1.8em;position:absolute;top:.05em;left:.17em;transform:rotate(45deg)}input[type=radio]:checked:after{content:" ";background-color:var(--bg);border-radius:100%;width:.25em;height:.25em;font-size:32px;position:absolute;top:.125em;left:.125em}@media only screen and (width<=720px){textarea,select,input{width:100%}}input[type=color]{height:2.5rem;padding:.2rem}input[type=file]{border:0}hr{background:var(--border);border:none;height:1px;margin:1rem auto}mark{border-radius:var(--standard-border-radius);background-color:var(--marked);color:#000;padding:2px 5px}mark a{color:#0d47a1}img,video{border-radius:var(--standard-border-radius);max-width:100%;height:auto}figure{margin:0;display:block;overflow-x:auto}figure>img,figure>picture>img{margin-inline:auto;display:block}figcaption{text-align:center;color:var(--text-light);margin-block:1rem;font-size:.9rem}blockquote{border-inline-start:.35rem solid var(--accent);color:var(--text-light);margin-block:2rem;margin-inline:2rem 0;padding:.4rem .8rem;font-style:italic}cite{color:var(--text-light);font-size:.9rem;font-style:normal}dt{color:var(--text-light)}code,pre,pre span,kbd,samp{font-family:var(--mono-font);color:var(--code)}kbd{color:var(--preformatted);border:1px solid var(--preformatted);border-bottom:3px solid var(--preformatted);border-radius:var(--standard-border-radius);padding:.1rem .4rem}pre{max-width:100%;color:var(--preformatted);padding:1rem 1.4rem;overflow:auto}pre code{color:var(--preformatted);background:0 0;margin:0;padding:0}progress{width:100%}progress:indeterminate{background-color:var(--accent-bg)}progress::-webkit-progress-bar{border-radius:var(--standard-border-radius);background-color:var(--accent-bg)}progress::-webkit-progress-value{border-radius:var(--standard-border-radius);background-color:var(--accent)}progress::-moz-progress-bar{border-radius:var(--standard-border-radius);background-color:var(--accent);transition-property:width;transition-duration:.3s}progress:indeterminate::-moz-progress-bar{background-color:var(--accent-bg)}dialog{background-color:var(--bg);max-width:40rem;margin:auto}dialog::backdrop{background-color:var(--bg);opacity:.8}@media only screen and (width<=720px){dialog{max-width:100%;margin:auto 1em}}sup,sub{vertical-align:baseline;position:relative}sup{top:-.4em}sub{top:.3em}.notice{background:var(--accent-bg);border:2px solid var(--border);border-radius:var(--standard-border-radius);margin:2rem 0;padding:1.5rem}@media print{@page{margin:1cm}body{display:block}body>header{background-color:unset}body>header nav,body>footer{display:none}article{border:none;padding:0}a[href^=http]:after{content:" <" attr(href)">"}abbr[title]:after{content:" (" attr(title)")"}a{text-decoration:none}p{widows:3;orphans:3}hr{border-top:1px solid var(--border)}mark{border:1px solid var(--border)}pre,table,figure,img,svg{break-inside:avoid}pre code{white-space:pre-wrap}}
)rawliteral";