// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/

//gets ip address
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
//event listener that calls the onload function when the web page loads
window.addEventListener('load', onload);
function onload(event) {
    initWebSocket();
}
//initialize a websocket connection with the server
function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

//handle message received via websocket protocol in onMessage fxn
function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    //if tempOrSPO2 is "SPO2", that means we are using pulse ox not temp sensor
	if(myObj.tempOrSPO2 == "SPO2")
	{
		document.getElementById("mySPO2").innerHTML = myObj.SPO2Data;
		document.getElementById("myPRbpm").innerHTML = myObj.PRbpm;
	}
	if(myObj.tempOrSPO2 == "temp")// tempOrSPO2 must be temp
	{
		document.getElementById("myTemp").innerHTML = myObj.temperature;
	}
	
}

/*When record temperature is clicked send mcu a message saying begin
 recording temperature "getTemp". Handle "getTemp" on esp side */
function tempButton(){
	websocket.send("getTemp");
}
function pulseOxButton(){
	websocket.send("getSPO2");
}