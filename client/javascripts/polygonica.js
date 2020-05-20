let viewer = null;
let len;
let cuttingSection = null;
const meshIDs = [];
let xDim = 0.0;
let yDim = 0.0;
let zDim = 0.0;
let hollowed = false;
let cutPlaneVisible;

let ServerURL = "https://cloud.techsoft3d.com/PgServer";

window.onload = function () {
	if (window.document.URL.includes("localhost")) {
		//len = window.document.URL.indexOf("demos") - 1;
		//ServerURL = window.document.URL.substring(0, len) + "/PgServer";
		ServerURL = "localhost:8888";

	}

	const pgReq = new XMLHttpRequest();
	pgReq.open("POST", ServerURL + "/api/run_pgserver");
	pgReq.onload = () => {
		console.log('PG Server is live');
	}
	pgReq.send();

	viewer = new Communicator.WebViewer({
		containerId: "container",
		empty: true
	});

	viewer.start();
	attachListeners();
	window.onresize = function () { viewer.resizeCanvas(); };
	window.onbeforeunload = function () {
		let oReq = new XMLHttpRequest();

		oReq.open("GET", ServerURL + "/CloseSession", true);
		oReq.responseType = "arraybuffer";
		oReq.onreadystatechange = function (oEvent) {
			if (oReq.readyState === XMLHttpRequest.DONE && oReq.status === 200) {
				console.log("Solid Destroyed");
				document.getElementById("hollow").innerHTML = Hollow;
				hollowed = false;
			}
		}
		oReq.send(null);

		$.ajax({
			url: ServerURL + '/api/close_pgserver',
			type: 'POST',
			dataType: 'json',
			data: {
				hello: 'hello'
			}
		})
	}

	let el = document.getElementById("container");
	if (el.addEventListener) {
		el.addEventListener("dragover", function (ev) { dragover_handler(ev); });
		el.addEventListener("drop", function (ev) { drop_handler(ev); });
	}
};

function deleteMesh() {
	if (meshIDs != null && meshIDs.length > 0) {
		viewer.getModel().deleteMeshInstances(meshIDs).then(function () {
			viewer.getModel().deleteMeshes();
		});
		meshIDs.length = 0;
	}
}

function drop_handler(ev) {
	document.getElementById("loading").style.visibility = "visible";
	ev.preventDefault();
	var dt = ev.dataTransfer;
	// If dropped items aren't files, reject them
	if (dt.items) {
		// Use DataTransferItemList interface to access the file(s)
		for (var i = 0; i < dt.items.length; i++) {
			if (dt.items[i].kind == "file") {
				var f = dt.items[i].getAsFile();
				console.log(f.name);

				if (f.name.split('.').pop().toUpperCase() == "HSF") {
					alert("HSF files are not currently supported");
					document.getElementById("loading").style.visibility = "hidden";
					return;
				}

				// First reset state of server before pushing new data
				var oReq = new XMLHttpRequest();
				oReq.open("GET", ServerURL + "/CloseSession", true);
				oReq.setRequestHeader("Access-Control-Allow-Origin", "*");
				oReq.responseType = "arraybuffer";
				oReq.onreadystatechange = function (oEvent) {
					if (oReq.readyState === XMLHttpRequest.DONE && oReq.status === 200) {

						document.getElementById("file_name").innerHTML = "File: " + f.name;

						var oReq2 = new XMLHttpRequest();
						oReq2.open("POST", ServerURL + "/" + f.name, true);
						oReq2.setRequestHeader("Content-type", "multipart/form-data"); // used for sending large data or binary data

						oReq2.responseType = "arraybuffer";

						oReq2.onreadystatechange = function () {//Call a function when the state changes.
							if (oReq2.readyState == XMLHttpRequest.DONE && oReq2.status == 200) {
								// Request finished. Do processing here.
								deleteMesh();
								console.log("File sent!!!");
								var float32Array = new Float32Array(oReq2.response);
								if (float32Array && float32Array.byteLength > 0) {
									var meshData = new Communicator.MeshData();
									meshData.setFaceWinding(Communicator.FaceWinding.CounterClockwise);
									meshData.addFaces(float32Array); // don't use normals
									viewer.getModel().createMesh(meshData).then(function (meshId) {
										var faceColor = new Communicator.Color(100, 100, 100);
										var meshInstanceData = new Communicator.MeshInstanceData(meshId, undefined, "ground_plane", faceColor);
										viewer.getModel().createMeshInstance(meshInstanceData).then(function (instacdId) {
											meshIDs.push(instacdId);
											viewer.fitWorld();
											viewer.getView().fitWorld();
											ShowErrors();
											UpdateModelStatus();
											UpdateModelStatistics();
											UpdatePrice();
											UpdateColor();
											document.getElementById("hollow").innerHTML = "Hollow";
											document.getElementById("hollow").style.backgroundColor = "White";
											hollowed = false;
										});
									});
								}
								else {
									document.getElementById("loading").style.visibility = "hidden";
									alert("There was a problem processing this file.");
								}

							}

						}
						oReq2.send(f);
					}
				}
				oReq.send(null);
			}
		}
	}
}

function dragover_handler(ev) {
	// Prevent default select and drag behavior
	ev.preventDefault();
}

function UpdateModelStatus() {
	var oReq = new XMLHttpRequest();
	oReq.open("GET", ServerURL + "/GetModelStatus", true);
	oReq.responseType = "arraybuffer";
	oReq.onreadystatechange = function (oEvent) {
		if (oReq.readyState === XMLHttpRequest.DONE && oReq.status === 200) {
			var intArray = new Int32Array(oReq.response);
			if (intArray && intArray.length == 4) {
				if (intArray[0]) {
					document.getElementById("closed").innerHTML = "True";
					document.getElementById("closed").style.color = "black";
				} else {
					document.getElementById("closed").innerHTML = "False";
					document.getElementById("closed").style.color = "red";
				}

				if (intArray[1]) {
					document.getElementById("manifold").innerHTML = "True";
					document.getElementById("manifold").style.color = "black";
				} else {
					document.getElementById("manifold").innerHTML = "False";
					document.getElementById("manifold").style.color = "red";
				}

				if (intArray[2]) {
					document.getElementById("no_intersections").innerHTML = "True";
					document.getElementById("no_intersections").style.color = "black";
				} else {
					document.getElementById("no_intersections").innerHTML = "False";
					document.getElementById("no_intersections").style.color = "red";
				}

				if (intArray[3]) {
					document.getElementById("correct_orientation").innerHTML = "True";
					document.getElementById("correct_orientation").style.color = "black";
				} else {
					document.getElementById("correct_orientation").innerHTML = "False";
					document.getElementById("correct_orientation").style.color = "red";
				}

				if (intArray[0] && intArray[1] && intArray[2] && intArray[3]) {
					document.getElementById("heal").disabled = true;
					document.getElementById("simplify").disabled = false;
					document.getElementById("hollow").disabled = false;
				} else {
					document.getElementById("heal").disabled = false;
					document.getElementById("simplify").disabled = true;
					document.getElementById("hollow").disabled = true;
				}
			}
		}
	}
	oReq.send(null);
}

function UpdateModelStatistics() {
	var oReq = new XMLHttpRequest();
	oReq.open("GET", ServerURL + "/GetModelStatistics", true);
	oReq.responseType = "arraybuffer";
	oReq.onreadystatechange = function (oEvent) {
		if (oReq.readyState === XMLHttpRequest.DONE && oReq.status === 200) {
			var floatArray = new Float32Array(oReq.response);
			if (floatArray && floatArray.length == 6) {
				var whole_number = Intl.NumberFormat('en-US',
					{
						style: 'decimal',
						minimumFractionDigits: 0
					});

				document.getElementById("point_count").innerHTML = whole_number.format(floatArray[0]);

				document.getElementById("face_count").innerHTML = whole_number.format(floatArray[1]);

				var dimensions;
				xDim = floatArray[2];
				yDim = floatArray[3];
				zDim = floatArray[4];
				dimensions = whole_number.format(xDim) + ' x ' +
					whole_number.format(yDim) + ' x ' +
					whole_number.format(zDim);

				document.getElementById("dimensions").innerHTML = dimensions;

				document.getElementById("volume").innerHTML = whole_number.format(floatArray[5]);
				UpdatePrice();
			}
		}
	}
	oReq.send(null);
}

function UpdatePrice() {
	var vol = xDim * yDim * zDim;
	var quant = parseFloat(quantity.value);

	var unit_multiplier = 1;

	var e = document.getElementById("units");
	if (e) {
		unit_multiplier = e.options[e.selectedIndex].value;
		unit_multiplier = unit_multiplier * unit_multiplier * unit_multiplier; // cube for volume
	}

	var material_cost = 1;
	e = document.getElementById("material");
	if (e) {
		material_cost = e.options[e.selectedIndex].value;
	}

	var finish_cost = 1;
	e = document.getElementById("finish");
	if (e) {
		finish_cost = e.options[e.selectedIndex].value;
	}

	var scale_cost = 1.0;
	e = document.getElementById("scale");
	if (e) {
		scale_cost = parseFloat(scale.value) / 100;
		scale_cost = scale_cost * scale_cost * scale_cost;
	}

	var uprice = vol * unit_multiplier * material_cost * finish_cost * scale_cost * .0003229; // some constant

	var price = Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD' });

	document.getElementById("unit_price").innerHTML = price.format(uprice);
	document.getElementById("total_price").innerHTML = price.format(uprice * quant);
}

function UpdateColor() {
	var e = document.getElementById("color");
	if (e) {
		var map = {};

		switch (e.selectedIndex) {
			case 0: // gray
				map[meshIDs[0]] = new Communicator.Color(128, 128, 128);
				viewer.getModel().setNodesColors(map);
				e.style.color = "gray"
				break;
			case 1: // red
				map[meshIDs[0]] = new Communicator.Color(255, 0, 0);
				viewer.getModel().setNodesColors(map);
				e.style.color = "red"
				break;
			case 2: // green
				map[meshIDs[0]] = new Communicator.Color(0, 255, 0);
				viewer.getModel().setNodesColors(map);
				e.style.color = "green"
				break;
			case 3: // blue
				map[meshIDs[0]] = new Communicator.Color(0, 0, 255);
				viewer.getModel().setNodesColors(map);
				e.style.color = "blue"
				break;
			case 4: // yellow
				map[meshIDs[0]] = new Communicator.Color(255, 255, 0);
				viewer.getModel().setNodesColors(map);
				e.style.color = "yellow"
				break;
			case 5: // brown
				map[meshIDs[0]] = new Communicator.Color(131, 92, 59);
				viewer.getModel().setNodesColors(map);
				e.style.color = "brown"
				break;
			case 6: // orange
				map[meshIDs[0]] = new Communicator.Color(255, 165, 0);
				viewer.getModel().setNodesColors(map);
				e.style.color = "orange"
				break;
			default:
		}
	}
}

function ShowErrors() {
	var oReq = new XMLHttpRequest();
	oReq.open("GET", ServerURL + "/GetOpenEdges", true);
	oReq.responseType = "arraybuffer";
	oReq.onreadystatechange = function (oEvent) {
		if (oReq.readyState === XMLHttpRequest.DONE && oReq.status === 200) {
			var float32Array = new Float32Array(oReq.response);
			if (float32Array) {
				var meshData = new Communicator.MeshData();
				for (let i = 0; i < float32Array.length; i += 6) {
					var tempFloatArray = new Float32Array(6);
					tempFloatArray[0] = float32Array[i];
					tempFloatArray[1] = float32Array[i + 1];
					tempFloatArray[2] = float32Array[i + 2];
					tempFloatArray[3] = float32Array[i + 3];
					tempFloatArray[4] = float32Array[i + 4];
					tempFloatArray[5] = float32Array[i + 5];
					meshData.addPolyline(tempFloatArray);
				}
				viewer.getModel().createMesh(meshData).then(function (meshId) {
					var lineColor = new Communicator.Color(0, 255, 0);
					let meshInstanceData = new Communicator.MeshInstanceData(meshId, undefined, "ground_line", undefined, lineColor);
					viewer.getModel().createMeshInstance(meshInstanceData).then(function (instacdId) {
						meshIDs.push(instacdId);
					});
				});
			}
		}
	}
	oReq.send(null);

	var oReq2 = new XMLHttpRequest();
	oReq2.open("GET", ServerURL + "/GetIntersectingTriangles", true);
	oReq2.responseType = "arraybuffer";
	oReq2.onreadystatechange = function (oEvent) {
		if (oReq2.readyState === XMLHttpRequest.DONE && oReq2.status === 200) {
			var float32Array = new Float32Array(oReq2.response);
			if (float32Array) {
				console.log(float32Array.length);
				console.log(float32Array[0]);
				var meshData = new Communicator.MeshData();
				meshData.setFaceWinding(Communicator.FaceWinding.CounterClockwise);
				meshData.addFaces(float32Array); // don't use normals
				viewer.getModel().createMesh(meshData).then(function (meshId) {
					var faceColor = new Communicator.Color(255, 0, 0);
					let meshInstanceData = new Communicator.MeshInstanceData(meshId, undefined, "ground_plane", faceColor);
					viewer.getModel().createMeshInstance(meshInstanceData).then(function (instacdId) {
						meshIDs.push(instacdId);
						var tempList = [];
						tempList.push(instacdId);
						viewer.getModel().setDepthRange(tempList, 0, .9999);
						document.getElementById("loading").style.visibility = "hidden";
					});
				});
			}
		}
	}
	oReq2.send(null);
}

function Heal() {
	document.getElementById("loading").style.visibility = "visible";
	document.getElementById("heal").disabled = true;
	document.getElementById("simplify").disabled = false;
	document.getElementById("hollow").disabled = false;

	var oReq = new XMLHttpRequest();
	oReq.open("GET", ServerURL + "/AutoHeal", true);
	oReq.responseType = "arraybuffer";

	oReq.onreadystatechange = function (oEvent) {
		if (oReq.readyState === XMLHttpRequest.DONE && oReq.status === 200) {
			var float32Array = new Float32Array(oReq.response);
			if (float32Array) {
				var meshData = new Communicator.MeshData();
				meshData.setFaceWinding(Communicator.FaceWinding.CounterClockwise);
				meshData.addFaces(float32Array); // don't use normals
				if (meshIDs.length > 0) {
					viewer.getModel().deleteMeshInstances(meshIDs).then(function (meshIds) {
						meshIDs.length = 0;
						viewer.getModel().createMesh(meshData).then(function (meshId) {
							var faceColor = new Communicator.Color(100, 100, 100);
							let meshInstanceData = new Communicator.MeshInstanceData(meshId, undefined, "ground_plane", faceColor);
							viewer.getModel().createMeshInstance(meshInstanceData).then(function (instacdId) {
								meshIDs.push(instacdId);
								document.getElementById("loading").style.visibility = "hidden";
								UpdateModelStatus();
								UpdateModelStatistics();
								UpdateColor();
							});
						});
					});
				}
			}
		}
	};
	oReq.send(null);
}

function Simplify() {
	document.getElementById("loading").style.visibility = "visible";

	var simplifyAmount = parseFloat(document.getElementById("simplify_percent").value);
	console.log("Simplify amount: " + simplifyAmount);

	var oReq = new XMLHttpRequest();
	oReq.open("POST", ServerURL + "/SetSimplifyAmount", true);
	oReq.setRequestHeader("Content-type", "multipart/form-data"); // used for sending large data or 

	oReq.onreadystatechange = function () {//Call a function when the state changes.
		if (oReq.readyState == XMLHttpRequest.DONE /*&& oReq.status == 200*/) { // TODO: figure out why 200 isn't being returned :(
			var oReq2 = new XMLHttpRequest();
			oReq2.open("GET", ServerURL + "/Simplify", true);
			oReq2.responseType = "arraybuffer";
			oReq2.onreadystatechange = function (oEvent) {
				if (oReq2.readyState === XMLHttpRequest.DONE && oReq2.status === 200) {
					var float32Array = new Float32Array(oReq2.response);
					if (float32Array) {
						var meshData = new Communicator.MeshData();
						meshData.setFaceWinding(Communicator.FaceWinding.CounterClockwise);
						meshData.addFaces(float32Array); // don't use normals
						if (meshIDs.length > 0) {
							viewer.getModel().deleteMeshInstances(meshIDs).then(function (meshIds) {
								meshIDs.length = 0;
								viewer.getModel().createMesh(meshData).then(function (meshId) {
									var faceColor = new Communicator.Color(100, 100, 100);
									let meshInstanceData = new Communicator.MeshInstanceData(meshId, undefined, "ground_plane", faceColor);
									viewer.getModel().createMeshInstance(meshInstanceData).then(function (instacdId) {
										meshIDs.push(instacdId);
										document.getElementById("loading").style.visibility = "hidden";
										UpdateModelStatistics();
										UpdateColor();
									});
								});
							});
						}
					}
				}
			};
			oReq2.send(null);
		}
	}
	console.log("Simplify amount 2: " + simplifyAmount);
	oReq.send(simplifyAmount);
}

function Hollow() {
	document.getElementById("loading").style.visibility = "visible";

	var hollowAmount = parseFloat(document.getElementById("offset").value);
	console.log(ServerURL);
	var oReq = new XMLHttpRequest();
	oReq.open("POST", ServerURL + "/SetHollowAmount", true);
	oReq.setRequestHeader("Content-type", "multipart/form-data"); // used for sending large data or 

	oReq.onreadystatechange = function () {//Call a function when the state changes.
		if (oReq.readyState == XMLHttpRequest.DONE /*&& oReq.status == 200*/) { // TODO: figure out why 200 isn't being returned :(
			var oReq2 = new XMLHttpRequest();
			oReq2.open("GET", ServerURL + "/Hollow", true);
			oReq2.responseType = "arraybuffer";
			oReq2.onreadystatechange = function (oEvent) {
				if (oReq2.readyState === XMLHttpRequest.DONE && oReq2.status === 200) {
					var float32Array = new Float32Array(oReq2.response);
					if (float32Array) {
						var meshData = new Communicator.MeshData();
						meshData.setFaceWinding(Communicator.FaceWinding.CounterClockwise);
						meshData.addFaces(float32Array); // don't use normals
						if (meshIDs.length > 0) {
							viewer.getModel().deleteMeshInstances(meshIDs).then(function (meshIds) {
								meshIDs.length = 0;
								viewer.getModel().createMesh(meshData).then(function (meshId) {
									var faceColor = new Communicator.Color(100, 100, 100);
									let meshInstanceData = new Communicator.MeshInstanceData(meshId, undefined, "ground_plane", faceColor);
									viewer.getModel().createMeshInstance(meshInstanceData).then(function (instacdId) {
										meshIDs.push(instacdId);
										document.getElementById("loading").style.visibility = "hidden";
										UpdateModelStatistics();
										UpdateColor();
										activateCuttingPlane();
										hollowed = true;
										cutPlaneVisible = true;
										document.getElementById("hollow").style.backgroundColor = "LightGray";
										document.getElementById("hollow").innerHTML = "Cutting Plane";
									});
								});
							});
						}
					}
				}
			};
			oReq2.send(null);
		}
	}
	oReq.send(hollowAmount);
}

function defineCuttingSection() {
	// get cutting manager
	var cuttingManager = viewer.getCuttingManager();
	// specify capping face color
	cuttingManager.setCappingFaceColor(new Communicator.Color(0, 255, 255));
	cuttingManager.setCappingGeometryVisibility(true);

	// create plane for cutting plane
	var plane = new Communicator.Plane();
	plane.normal.set(1, 0, 0);
	plane.d = 0;

	// create array for cutting reference geometry
	var referenceGeometry = [];

	viewer.getModel().getModelBounding(true, false).then(function (bounding) {

		var dy = (bounding.max.y - bounding.min.y) * .2;
		var dz = (bounding.max.z - bounding.min.z) * .2;

		var x = 0;
		plane.d = -bounding.min.x - (bounding.max.x - bounding.min.x) / 2;

		referenceGeometry.push(new Communicator.Point3(x, bounding.min.y - dy, bounding.min.z - dz));
		referenceGeometry.push(new Communicator.Point3(x, bounding.min.y - dy, bounding.max.z + dz));
		referenceGeometry.push(new Communicator.Point3(x, bounding.max.y + dy, bounding.max.z + dz));
		referenceGeometry.push(new Communicator.Point3(x, bounding.max.y + dy, bounding.min.z - dz));

		// get cuting section and activate
		cuttingSection = cuttingManager.getCuttingSection(0);   // index is predefined 0 ~ 3
		cuttingSection.addPlane(plane, referenceGeometry);
		cuttingSection.activate();
	});
}

function activateCuttingPlane() {
	if (cuttingSection == null) {
		defineCuttingSection();
	}
}

function deactivateCuttingPlane() {
	if (cuttingSection != undefined) {
		cuttingSection.deactivate();
	}
}

function onHollowOk() {
	document.getElementById("hollow-dialog").style.visibility = "hidden";
	Hollow();
}

function onHollowCancel() {
	document.getElementById("hollow-dialog").style.visibility = "hidden";
}

function onHollow() {
	if (!hollowed) {
		var float_number = Intl.NumberFormat('en-US', { style: 'decimal', minimumFractionDigits: 3 });
		var max = Math.max(Math.max(xDim, yDim), zDim);
		var off = max * 0.02;
		var val = float_number.format(off);
		document.getElementById("offset").value = val;
		document.getElementById("hollow-dialog").style.visibility = "visible";
	} else {
		if (cutPlaneVisible) {
			cutPlaneVisible = false;
			deactivateCuttingPlane();
			document.getElementById("hollow").style.backgroundColor = "white";
		} else {
			cutPlaneVisible = true;
			cuttingSection.activate();
			document.getElementById("hollow").style.backgroundColor = "LightGray";
		}
	}
}

function onSimplify() {
	document.getElementById("simplify-dialog").style.visibility = "visible";
}

function onSimplifyOk() {
	document.getElementById("simplify-dialog").style.visibility = "hidden";
	Simplify();
}

function onSimplifyCancel() {
	document.getElementById("simplify-dialog").style.visibility = "hidden";
}

function onCheckout() {
	document.getElementById("checkout-dialog").style.visibility = "visible";
}

function onCheckoutClose() {
	document.getElementById("checkout-dialog").style.visibility = "hidden";
}

const attachListeners = () => {
	document.getElementById('heal').addEventListener('click', () => Heal());
	document.getElementById('simplify').addEventListener('click', () => onSimplify());
	document.getElementById('simplify-settings-ok-button').addEventListener('click', () => onSimplifyOk());
	document.getElementById('simplify-settings-cancel-button').addEventListener('click', () => onSimplifyCancel());
	document.getElementById('hollow').addEventListener('click', () => onHollow());
	document.getElementById('hollow-settings-ok-button').addEventListener('click', () => onHollowOk());
	document.getElementById('hollow-settings-cancel-button').addEventListener('click', () => onHollowCancel());
	document.getElementById('checkout-open-button').addEventListener('click', () => onCheckout());
	document.getElementById('checkout-close-button').addEventListener('click', () => onCheckoutClose());
	["units", "material", "finish", "scale", "quantity"].forEach((target) => {
		switch (target) {
			case 'units':
			case 'material':
			case 'finish':
				document.getElementById(target).addEventListener('change', () => UpdatePrice());
				break;
			case 'scale':
			case 'quantity':
				document.getElementById(target).addEventListener('click', () => UpdatePrice());
				break;
		}

	});
	document.getElementById('scale').addEventListener('keyup', () => UpdatePrice());
	document.getElementById('color').addEventListener('change', () => {
		UpdateColor();
	});
}