//This script is used in RemoteMe.org to send messages to the ESP-32-CAM, and change displayed image on the web page.

var cameraDeviceId=5;

function takePhotoNow(){
	RemoteMe.getInstance().sendUserMessage(cameraDeviceId ,[false]);
	$("#progress").css("display","block");
}

function takeFlashPhotoNow() {
	RemoteMe.getInstance().sendUserMessage(cameraDeviceId ,[true]);
	$("#progress").css("display","block");
}


$(document).ready(function () {
	let remoteme=RemoteMe.getInstance();//connect to RemoteMe and keeps conenction live

	let fileName = "photos/photo.jpg";
	let deviceId = thisDeviceId;

	let image = $('#imageForPhoto');

	image[0].src=`/wp/device_${deviceId}/${fileName}?r=${Math.floor(Math.random() * 10000)}`;

	image[0].onload = function() {
		$("#progress").css("display","none");
	};

	remoteme.remoteMeConfig.deviceFileChange.push((rdeviceId,rfileName)=>{
		if ((deviceId==rdeviceId)&&(rfileName==fileName)){
			image[0].src=`/wp/device_${deviceId}/${fileName}?r=${Math.floor(Math.random() * 10000)}`;

		}
	});

	remoteme.subscribeEvent(EventSubscriberTypeEnum.FILE_CHANGE);
});
