// Complete project details: https://randomnerdtutorials.com/esp32-web-server-websocket-sliders/[1]
//[1]Begin----------------------------------------------------------------------------
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
//[1] end---------------------------------------------------------------------------
//handle message received via websocket protocol in onMessage fxn
function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);//search JSON string sent from MCU to website
    //if tempOrSPO2 is "SPO2", that means we are using pulse ox not temp sensor
	if(myObj.tempOrSPO2 == "SPO2")
	{
		//place SPO2 value from JSON string in mySPO2 value of HTML website
		document.getElementById("mySPO2").innerHTML = myObj.SPO2Data;
		//place bpm value from JSON string in myPRbpm value of HTML website
		document.getElementById("myPRbpm").innerHTML = myObj.PRbpm;
	}
	//if tempOrSPO2 is "SPO2", that means we are using temperature sensor
	if(myObj.tempOrSPO2 == "temp")
	{
		//place temperature value from JSON string in myTemp value of HTML website
		document.getElementById("myTemp").innerHTML = myObj.temperature;
	}
	return;
}

/*When record temperature is clicked send mcu a message saying begin
 recording temperature "getTemp". Handle "getTemp" on esp side */
function tempButton(){
	websocket.send("getTemp");
}
/*When record SPO2 is clicked send mcu a message saying begin
 recording temperature "getSPO2". Handle "getSPO2" on esp side */
function pulseOxButton(){
	websocket.send("getSPO2");
}
