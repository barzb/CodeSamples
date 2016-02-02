// MESH BRUSH by Benjamin Barz (barzb@hochschule-trier.de)
using UnityEngine;
using System.Collections;
using UnityEditor;
using System.IO;

class MeshBrush : EditorWindow {

	// ---------------------- ATTRIBUTES ----------------------//

	// mesh component references
	MeshFilter 	 meshFilter		= null; // mesh input field
	MeshFilter	 meshFilterCopy	= null; // for mesh-change check
	Mesh 		 mesh	 		= null; // mesh of the meshFilter
	MeshCollider collider		= null; // collider of the mesh
	Texture2D 	 texture 		= null; // main object of this script

	// texture size and selected tab in editor window
	int texWidth 		= 0; 
	int texHeight 		= 0;
	int tab 			= 0;

	// attributes from texture mode
	Texture2D brushTex 	= null;	 // texture to draw
	Texture2D brush 	= null;	 // texture of the brush
	bool closedMesh		= false; // for over-the-edge brushing
	float drawSize 		= 1.0f;	 // size of the brush
	float drawRatio 	= 1.0f;	 // scale-factor for width of the brush
	float texSize  		= 1.0f;	 // scale of the brushed texture

	// attributes from object mode
	GameObject placeObj = null;	 // object to place on the mesh
	float population	= 10f;	 // how frequent objects will be placed
	float objSize		= 1f;	 // scale factor of the object
	bool rotateObj		= true;	 // if object is aligned to surface-normal
	float placeTimer	= 0f;	 // timer for when a new object is placed

	// state attributes and others
	bool saved			= false;	 // if the texture was saved after changing
	bool drawMode 		= false;	 // you can only brush or place in draw mode
	bool mouseRelease 	= true;	 	 // for mouse release event
	Tool lastTool 		= Tool.None; // last unity tool used


	// ---------------------- GUI CONTENTS -------------------//
	// GUIContent( label, tooltip )

	// mesh 
	GUIContent ui_meshField			= new GUIContent("Mesh"			 ,"Select the mesh you want to draw on.");

	// create-texture mode
	GUIContent ui_texWidth			= new GUIContent("Texture Width" ,"Width in pixels of the created texture.");
	GUIContent ui_texHeight			= new GUIContent("Texture Height","Height in pixels of the created texture.");
	GUIContent ui_createTexButton	= new GUIContent("Create Texture","Create the new texture for the mesh and save it as png.");

	// tabs
	string[]   ui_tabNames			= new string[]  {"Texture Mode"	 ,"Object Mode"};

	// texture mode
	GUIContent ui_drawButtonStart	= new GUIContent("Start Drawing" ,"Draw on the selected mesh.");
	GUIContent ui_drawButtonStop	= new GUIContent("Stop Drawing"  ,"Stop the drawing mode.");
	GUIContent ui_floodButton		= new GUIContent("Flood Texture" ,"Flood the whole mesh with the selected texture.");
	GUIContent ui_textureField		= new GUIContent("Texture"		 ,"Select the texture you like to draw on the mesh.");
	GUIContent ui_brushField		= new GUIContent("Brush"		 ,"Select the brush-texture you like to draw with.");
	GUIContent ui_textureSize		= new GUIContent("Texture Size"	 ,"Scale of the selected texture.");
	GUIContent ui_brushSize			= new GUIContent("Brush Size"	 ,"Scale of the brush.");
	GUIContent ui_closedMesh	    = new GUIContent("Closed Mesh"	 ,"Enable to draw over the edges of the texture.");
	GUIContent ui_saveTextureButton = new GUIContent("Save Texture"	 ,"Save all the changes of the texture to the .png file.");
	GUIContent ui_newTextureButton	= new GUIContent("New Texture"	 ,"Recreate the texture.");
	GUIContent ui_brushRatio		= new GUIContent("Brush Ratio"	 ,"Width of the brushed texture.\nA value of 1 is ratio 1:1" +
	                                           		 				  "\nThis is for stretching texture on non-square meshes.");
	// object mode
	GUIContent ui_placeButtonStart	= new GUIContent("Start Placing" ,"Place objects on the selected mesh.");
	GUIContent ui_placeButtonStop	= new GUIContent("Stop Placing"  ,"Stop the object placement mode.");
	GUIContent ui_objectField		= new GUIContent("Object"		 ,"Select the prefab you like to place on the mesh.");
	GUIContent ui_objSize			= new GUIContent("Object Size"	 ,"Scale of the selected object.");
	GUIContent ui_population		= new GUIContent("Population"	 ,"How many Objects will be placed in one placement.");
	GUIContent ui_rotateObj			= new GUIContent("Align Rotation","Align the rotation of the Object to the mesh normal.");


	// ----------------------- METHODS -----------------------//

	// set menu entry to Tools->MeshBrush
	[MenuItem ("Tools/MeshBrush")]
	public static void  ShowWindow () {
		EditorWindow.GetWindow(typeof(MeshBrush));
	}

	// set the scene for OnSceneGUI calls
	void OnFocus() {
		// Remove delegate listener if it has previously been assigned.
		SceneView.onSceneGUIDelegate -= this.OnSceneGUI;
		
		// Add (or re-add) the delegate.
		SceneView.onSceneGUIDelegate += this.OnSceneGUI;
	}

	// remove the scene
	void OnDestroy() {
		// When the window is destroyed, remove the delegate so that it will no longer do any drawing.
		SceneView.onSceneGUIDelegate -= this.OnSceneGUI;
	}


	/* 
	 * method is called, when editor window has focus
	 * draw GUI in each frame when active
	*/
	void OnGUI () {
		// change background color of the editor window to gray, when draw mode is active
		if(drawMode)
		{
			Rect background = new Rect(0f, 0f, position.width, position.height);
			Texture2D backgroundTexture = new Texture2D(1, 1, TextureFormat.RGBA32, false);
			backgroundTexture.SetPixel(0, 0, Color.gray);
			backgroundTexture.Apply();
			GUI.DrawTexture(background, backgroundTexture);
			DestroyImmediate(backgroundTexture);
		}

		// First elements "label" and "mesh field" showed in every tab; get meshFilter object
		GUILayout.Label ("Chose mesh", EditorStyles.boldLabel);
		meshFilter = EditorGUILayout.ObjectField(ui_meshField, meshFilter, typeof(MeshFilter), true) as MeshFilter;

		// if an object has been selected
		if(meshFilter != null)
		{
			// and selected object has a mesh
			if(meshFilter.sharedMesh != null) 
			{
				// called, when the meshFilter was freshly selected or changed
				if(mesh == null || mesh != meshFilter.sharedMesh)
				{
					// called, when mesh has been changed but the texture has not been saved
					if(texture != null && !saved && EditorUtility.DisplayDialog("Save changes?", 
						"The changes you made have not been saved.\nThey may be displayed but the .png file won't contain them." +
						"\nSave texture?",
					    "Yes", "No")
					) {
						// get previous mesh components
						MeshFilter newMeshFilter = meshFilter;
						meshFilter = meshFilterCopy;
						mesh = meshFilter.sharedMesh;
						// save the texture
						SaveTexture(false);
						// change to the new mesh again
						meshFilter = newMeshFilter;
					}

					// set previous meshFilter reference to current for change-checks
					meshFilterCopy = meshFilter;

					// disable draw mode and assign other mesh components
					setDrawMode(false);
					mesh = meshFilter.sharedMesh;
					collider = meshFilter.GetComponent<MeshCollider>();

					// try to load an existing texture for this mesh
					LoadTexture();

					// not needed anymore
					texWidth = 0;
					texHeight = 0;
				}  

				// when no meshcollider is referenced, try to load from mesh
				if(collider == null)
					collider = meshFilter.GetComponent<MeshCollider>();

				// called, when mesh has no meshcollider
				if(collider == null || collider.GetType() != typeof(MeshCollider))
				{
					// disable draw mode and show dialog for mesh-collider-creation
					setDrawMode(false);
					if(EditorUtility.DisplayDialog("No mesh collider", 
					   	"Your selected object has no mesh collider. \nCreate? ...You can delete it after texturing.",
					   	"Yes", "No")
					) {
						// user wants to create mesh-collider -> create it!
						meshFilter.gameObject.AddComponent<MeshCollider>();
						meshFilter.GetComponent<MeshCollider>().sharedMesh = meshFilter.sharedMesh;
					}
					else
					{
						// user doesn't want to create collider -> reset references
						meshFilter 	= null;
						mesh 		= null;
						texture 	= null;
						collider 	= null;
					}
				}
				GUILayout.Space(10);

				// called, when a texture was created and assigned
				if(texture != null)
				{
					// get current tab id
					tab = GUILayout.Toolbar (tab, ui_tabNames);
					switch (tab) {

					// -------------- TEXTURE MODE -------------- //
					case 0:
						// start/stop draw mode button
						if(GUILayout.Button(drawMode ? ui_drawButtonStop : ui_drawButtonStart))
						{
							setDrawMode(!drawMode);
							if(drawMode) {
								if(brush == null)
									Debug.Log("No brush selected");
								if(brushTex == null)
									Debug.Log("No texture selected");
							}
						}

						// show warning label, when texture changes has not been saved
						if(!saved)
						{
							GUILayout.Label ("Texture has been modified." +
								"\nRemember to save it after you are finished " +
								"\nor all changes will be dismissed.", EditorStyles.boldLabel
							);
						}

						// flood texture button
						GUILayout.Space(10);
						if(GUILayout.Button(ui_floodButton))
						{
							if(brushTex == null)
								Debug.Log("No texture selected");
						
							Undo.RegisterCompleteObjectUndo(texture, "Flood Texture");
							setDrawMode(false);
							floodTexture();
						}

						// texture object field
						GUILayout.Space(10);
						brushTex = EditorGUILayout.ObjectField(ui_textureField, brushTex, typeof(Texture2D), false) as Texture2D;

						// brush-texture object field
						GUILayout.Space(10);
						brush = EditorGUILayout.ObjectField(ui_brushField, brush, typeof(Texture2D), false) as Texture2D;

						// texture size slider
						GUILayout.Space(10);
						texSize = EditorGUILayout.Slider(ui_textureSize, texSize, 0.01f, 3.0f);

						// brush size slider
						GUILayout.Space(5);
						drawSize = EditorGUILayout.Slider(ui_brushSize, drawSize, 0.02f, 2.0f);

						// draw ratio slider
                    	GUILayout.Space(5);
                    	drawRatio = EditorGUILayout.Slider(ui_brushRatio, drawRatio, -0.02f, 5f);

						// closed mesh checkbox
						GUILayout.Space(5);
						closedMesh = EditorGUILayout.Toggle(ui_closedMesh, closedMesh);

						// save texture button
						GUILayout.Space(10);
						if(GUILayout.Button(ui_saveTextureButton))
						{
							setDrawMode(false);
							// call save method, parameter; [false] = save existing texture
							SaveTexture(false);
						}

						// new texture button
						GUILayout.Space(10);
						if(GUILayout.Button(ui_newTextureButton))
						{
							// show confirm dialog
							if(EditorUtility.DisplayDialog("Confirm action", 
							                               "Do you really want to overwrite your created texture?",
							                               "Yes", "No")
							)
							{	// the create-texture interface will be shown next frame

								Undo.RegisterCompleteObjectUndo(texture, "New Texture");
								
								texWidth = 0;
								texHeight = 0;
								
								setDrawMode(false);
								texture = null;
							}
						}
						break;
					
					// -------------- OBJECT MODE -------------- //
					case 1:
						// disable draw mode when no object to place is assigned
						if(placeObj == null)
							setDrawMode(false);

						// start/stop draw mode button
						if(GUILayout.Button(drawMode ? ui_placeButtonStop : ui_placeButtonStart))
						{
							if(placeObj == null)
								Debug.Log("No object selected");
							else
								setDrawMode(!drawMode);
						}

						// object-to-place field
						GUILayout.Space(10);
						placeObj = EditorGUILayout.ObjectField(ui_objectField, placeObj, typeof(GameObject), false) as GameObject;

						// object size slider
						GUILayout.Space(10);
						objSize = EditorGUILayout.Slider(ui_objSize, objSize, 0.01f, 100.0f);

						// placement radius slider
						GUILayout.Space(5);
						drawSize = EditorGUILayout.Slider(ui_brushSize, drawSize, 0.02f, 2.0f);

						// object frequency slider
						GUILayout.Space(5);
						population = EditorGUILayout.Slider(ui_population, population, 1f, 100f);

						// rotation aligned to mesh-surface normal checkbox
						GUILayout.Space(5);
						rotateObj = EditorGUILayout.Toggle(ui_rotateObj, rotateObj);

						break;
					}

				} 
				// called, when mesh has no created texture or new texture button was pressed
				else 
				{
					// try to read the mesh-size for suggested width/height of texture
					GUILayout.Space(10);
					if(texWidth == 0)
						texWidth  = (int)mesh.bounds.size.x*100;
					if(texHeight == 0)
						texHeight = (int)mesh.bounds.size.z*100;

					// texture width and heigth input field
					texWidth  = EditorGUILayout.IntField(ui_texWidth, texWidth);
					texHeight = EditorGUILayout.IntField(ui_texHeight, texHeight);
	
					// create texture button
					GUILayout.Space(10);
					if(GUILayout.Button(ui_createTexButton))
					{
						// set component references
						mesh = meshFilter.sharedMesh;
						collider = meshFilter.GetComponent<MeshCollider>();
						// call save method; [true] = create new texture
						SaveTexture(true);
					}
				}
			}

			// called, when meshFilter does not contain a mesh
			else
			{
				GUILayout.Label ("No mesh found.", EditorStyles.boldLabel);
				setDrawMode(false);
			}

		// called, when no meshFilter object was assigned to the script
		} else {
			setDrawMode(false);
		}

	}


	/* 
	 * method is called, when scene window has focus
	*/
	void OnSceneGUI(SceneView sceneView) {
		// if draw mode is active -> disable all other mouse actions in the scene (select object, ...)
		if(drawMode)
			HandleUtility.AddDefaultControl(GUIUtility.GetControlID(FocusType.Passive));
		// else -> leave this method because you don't want to draw anything...
		else
			return;

		// cast a ray from the current mouse position
		Ray ray = HandleUtility.GUIPointToWorldRay(Event.current.mousePosition);
		RaycastHit hit = new RaycastHit();

		// if the ray hits our selected mesh -> draw the red circle around the mouse position
		if (Physics.Raycast(ray, out hit, 1000.0f) && hit.transform.gameObject == meshFilter.gameObject) 
		{
			Handles.color = Color.red;
			// align the rotation of the circle to the mesh-surface normal
			Handles.CircleCap(0, hit.point, Quaternion.LookRotation(hit.normal), drawSize/2);
			HandleUtility.Repaint();
		}

		// called, when left mouse button is pressed
		if (drawMode && texture != null && Event.current.button == 0)
		{
			if (Event.current.type == EventType.MouseDown || Event.current.type == EventType.MouseDrag)
			{
				MeshCollider c = hit.collider as MeshCollider;

				// our meshCollider wasn't hit -> leave this method
				if (c == null || c != collider) {
					return;
				}

				// texture mode
				if(tab == 0) {
					// get texture coordinates
					Vector2 px = hit.textureCoord;
					int px_x = (int)(texture.width * px.x);
					int px_y = (int)(texture.height * px.y);

					// call draw pixel block method
					drawPixelBlock(px_x, px_y);
				} 
				// object mode
				else {
					// call place objectblock  method
					placeObjectBlock();
				}

				// mouse button pressed disables the mouseRelease boolean
				mouseRelease = false;
			}
			// mouse up event
			if (Event.current.type == EventType.MouseUp)
			{
				// enable mouseRelease boolean and reset placeTimer for object mode
				mouseRelease = true;
				placeTimer = 0;
			}
		}
	}


	/*
	 * --- TEXTURE MODE ---
	 * flood the mesh with the selected drawing-texture
	 * every pixel will be painted
	*/
	void floodTexture()
	{
		// loop counter for painted pixel row and column in block
		int i = 0;
		int j = 0;

		// default color
		Color color = Color.white;

		// loop through all pixels of |<----X---->| and |<----Y---->| from mousePosition
		for (int x = 0; x < texture.width; x++) 
		{
			// reset col counter in every new row
			i = 0;
			for (int y = 0; y < texture.height; y++) 
			{
				// called, when a drawing-texture was selected
				if(brushTex != null)
				{
					// scale (and stretch) the drawing-texture according to texture size slider and
					// draw ratio slider and get the relevant pixel color of the drawing-texture
					color = brushTex.GetPixel((int)(1/drawRatio * x/texSize), (int)(y/texSize));				
				}
				// set the selected pixel color of the mesh texture
				texture.SetPixel(x, y, color);

				// increment col counter
				i++;
			}
			// increment row counter
			j++;
		}

		// apply all SetPixels, recalculate mip levels = [true]
		texture.Apply(true);
		
		// mesh has been changed and must be saved
		saved = false;
	}

	
	/*
	 * --- TEXTURE MODE ---
	 * draw a block of pixels on the texture with the selected brush and texture
	 * at the clicked texture coordinates
	*/
	void drawPixelBlock(int x, int y)
	{
		// this method will be called every frame, the left mouse button is pressed
		// but you only want to register the object at the first call until the
		// mouse is released again
		if(mouseRelease)
        	Undo.RegisterCompleteObjectUndo(texture, "Use Texture Brush");

		// loop counter for painted pixel row and column in block
		int i = 0;
		int j = 0;

		// radius of the brush depending on brush size from gui slider
		// width of pixel block will be scaled with drawRatio from gui slider
		// drawRatio = 1 will result in a 1:1 ratio of width:height
		int brushRadiusX = (int)(100*drawSize*drawRatio);
		int brushRadiusY = (int)(100*drawSize);

		// default color
		Color color = Color.white;

		// loop through all pixels of |<----X---->| and |<----Y---->| from mousePosition
		for (int X = x-brushRadiusX; X < x+brushRadiusX; X++) 
		{
			// reset col counter in every new row
			i = 0;
			for (int Y = y-brushRadiusY; Y < y+brushRadiusY; Y++) 
			{
				// called, when a drawing-texture was selected
				if(brushTex != null)
				{
					// scale (and stretch) the drawing-texture according to texture size slider and
					// draw ratio slider and get the relevant pixel color of the drawing-texture
					color = brushTex.GetPixel((int)(1/drawRatio * X/texSize), (int)(Y/texSize));

					// called, when a brush-texture was selected
					if(brush != null)
					{
						// stretch the brush-texture to the pixel-block and get 
						// relevant pixel coordinate of the brush-texture
						int brushX = (int)(1.0f*i/brushRadiusY/2*brush.width);
						int brushY = (int)(1.0f*j/brushRadiusX/2*brush.height);

						// brush-texture should be black/white -> we take the RED 
						// value of the relevant pixel
						float alphaFromBrush = brush.GetPixel(brushX, brushY).r;

						// new color interpolated between old pixel color of the mesh texture 
						// and the color of the drawing-texture with brush-pixel-alpha
						color = interpolateColor(texture.GetPixel(X, Y), color, alphaFromBrush);
					}
					
				}
				// change the selected pixel color of the texture, but do not draw over the edge
				// of the texture, when our mesh is not closed
                if (closedMesh || (X >= 0 && X < texture.width && Y >= 0 && Y < texture.height))
                    texture.SetPixel(X, Y, color);
                
				// increment col counter
				i++;
			}
			// increment row counter
			j++;
		}

		// apply all pixel color changes, recalculate mip levels = [true]
		texture.Apply(true); 
		
		// mesh has been changed and must be saved
		saved = false;
	}


	/*
	 * --- OBJECT MODE ---
	 * place an object from the gui object field on the mesh
	 * cast a ray at random position in selected radius from
	 * current mouse position
	*/
	void placeObjectBlock()
	{
		// increment placeTimer depending on population counter gui element
		placeTimer += population;

		// if the timer is not high enough, leave this method
		if(placeTimer < 100f)
			return;

		// hooray! an object can be placed, now reset the timer
		placeTimer = 0f;

		// get current mouse position
		Vector2 p = Event.current.mousePosition;

		// multiplicator for distance from mouse position radius
		int m = 50;

		// generate a random coordinate
		float posX = Random.Range(p.x - drawSize*m, p.x + drawSize*m);
		float posY = Random.Range(p.y - drawSize*m, p.y + drawSize*m);

		// cast another ray at this coordinate
		Ray ray = HandleUtility.GUIPointToWorldRay(new Vector2(posX, posY));
		RaycastHit hit = new RaycastHit();

		// check if ray hits our mesh
		if (Physics.Raycast(ray, out hit, 1000.0f) && hit.transform.gameObject == meshFilter.gameObject) 
		{		
			// rotation of the object
			Quaternion rot;
		
			if(rotateObj) {
				// called, when align object gui checkbox was enabled
				// we calculate the rotation from the mesh-surface normal
				rot = Quaternion.FromToRotation(Vector3.up, hit.normal);
			} else {
				// called, when we don't want to align the rotation
				// use the default rotation of the placed object instead
				rot = Quaternion.identity;
			}

			// instantiate an object at the hit position
			GameObject o = Instantiate(placeObj, hit.point, rot) as GameObject;
			Undo.RegisterCreatedObjectUndo(o, "Place Objects");

			// set mesh object as parent of the created object
			o.transform.SetParent(meshFilter.transform);
			// and scale it depending on object size gui slider
			o.transform.localScale *= objSize;
		} 

	}


	/*
	 * Interpolates between colors c1 and c2 by v.
	 * v is between 0 and 1. 
	 * When v is 1 returns c1. When v is 0 returns c2.
	*/
	private Color interpolateColor(Color c1, Color c2, float v)
	{
        Color v1 = new Color(v, v, v);
        Color v2 = new Color(1 - v, 1 - v, 1 - v);

        return v1 * c1 + v2 * c2;
    }


	/*
	 * enable/disable the draw mode for brushing texture
	 * or placing objects on the mesh
	*/
	void setDrawMode(bool active)
	{
		// set draw mode boolean
		drawMode = active;
		
		if(active)
		{
			// called, when draw mode is enabled
			// save the current selected unity editor tool
			// and unequip the tool
			lastTool = Tools.current;
			Tools.current = Tool.None;
		} else {
			// called, when draw mode is disabled
			// equip the previous saved unity editor tool
			Tools.current = lastTool;
		}

		// called, when there is an meshFilter in the object field
		if(meshFilter != null)
		{
			// get the renderer of the mesh
			Renderer r = meshFilter.gameObject.GetComponent<Renderer>();

			// if there is a renderer -> hide/show wireframe of the object
			if(r != null)
				EditorUtility.SetSelectedWireframeHidden(r, active);
		}

		// repaint the scene
		SceneView.RepaintAll();
	}


	/*
	 * try to load a texture for the mesh created earlier
	 * when no texture was created, return false
	*/
	bool LoadTexture()
	{
		// name of the texture file contains the name of the meshFilter and the mesh
		Texture2D loadedTexture = EditorGUIUtility.Load(meshFilter.name+"_"+mesh.name+".png") as Texture2D;

		// set the texture reference
		if(texture != loadedTexture) {
			texture = loadedTexture;
		}

		// no changes on texture has been made
		saved = true;

		// return if successful
		return (texture != null);
	}


	/*
	 * save the texture to a .png file
	 * if newTexture is [true] -> create a new Texture
	*/
	void SaveTexture(bool newTexture) 
	{
		saved = true;
		
		Texture2D tex;

		if(newTexture)
		{	// called, when we want to create a new texture
			// create a texture the size of selected width+height gui fields, RGB24 format
			tex = new Texture2D(texWidth, texHeight, TextureFormat.RGB24, false);
		} else {
			// called, when there is an existing texture we just want to overwrite
			tex = texture;
		}

		
		// encode texture into PNG
		byte[] bytes = tex.EncodeToPNG();

		// set path and name variables
		// tPath now has the default path of the EdiitorGUIUtility.Load function
		string tPath = "/Editor Default Resources/";
		string tName = meshFilter.name+"_"+mesh.name;

		// create the default path for textures
		System.IO.Directory.CreateDirectory(Application.dataPath + tPath);

		// save the encoded texture file
		File.WriteAllBytes(Application.dataPath + tPath + tName + ".png", bytes);

		// refresh the asset database
		AssetDatabase.Refresh();

		// create material with diffuse shader
		Material m = new Material(Shader.Find("Diffuse"));

		// reload the texture from the file
		tex = EditorGUIUtility.Load(meshFilter.name+"_"+mesh.name+".png") as Texture2D;

		// change the texture format to advanced and set read/write enabled true
		SetTextureImporterFormat(tex, true);

		// reload the recreated texture from the file, so we can assign it to the material
		if(LoadTexture())
		{		
			// set the texture reference of this script
			texture = tex;
		}

		// assign the texture to the material
		m.mainTexture = tex;

		// create the material as asset
		AssetDatabase.CreateAsset(m, "Assets/Materials/" + tName + ".mat");

		// assign the material to the meshs renderer
		Renderer r = meshFilter.gameObject.GetComponent<Renderer>();
		r.material = m;
	}


	/*
	 * change the texture format to advanced and set read/write enabled true
	*/
	static void SetTextureImporterFormat(Texture2D texture, bool isReadable)
	{
		// leave the method when texture reference not set
		if ( null == texture ) return;

		// get path of the asset
		string assetPath = AssetDatabase.GetAssetPath( texture );

		// create texture importer object
		TextureImporter tImporter = AssetImporter.GetAtPath( assetPath ) as TextureImporter;
		if ( tImporter != null )
		{
			// set type to advanced
			tImporter.textureType = TextureImporterType.Advanced;
			// set format to RGB24
			tImporter.textureFormat = TextureImporterFormat.RGB24;
			// enable read/write
			tImporter.isReadable = isReadable;

			// reimport asset
			AssetDatabase.ImportAsset( assetPath );
			// refesh database
			AssetDatabase.Refresh();
		}
	}
}




