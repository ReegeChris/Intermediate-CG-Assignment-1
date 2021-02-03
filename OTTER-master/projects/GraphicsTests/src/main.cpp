//Just a simple handler for simple initialization stuffs
#include "BackendHandler.h"

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <Texture2D.h>
#include <Texture2DData.h>
#include <MeshBuilder.h>
#include <MeshFactory.h>
#include <NotObjLoader.h>
#include <ObjLoader.h>
#include <VertexTypes.h>
#include <ShaderMaterial.h>
#include <RendererComponent.h>
#include <TextureCubeMap.h>
#include <TextureCubeMapData.h>

#include <Timing.h>
#include <GameObjectTag.h>
#include <InputHelpers.h>

#include <IBehaviour.h>
#include <CameraControlBehaviour.h>
#include <FollowPathBehaviour.h>
#include <SimpleMoveBehaviour.h>



int main() {
	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default
	std::vector<GameObject> controllables;

	BackendHandler::InitAll();

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(BackendHandler::GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui
		
		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		shader->Link();

		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 2.0f);
		glm::vec3 lightCol = glm::vec3(1.0f, 0.85f, 0.5f);
		float     lightAmbientPow = 1.0f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.1f;
		float     lightLinearFalloff = 0.09f;
		float     lightQuadraticFalloff = 0.032f;
		
		//Bool variables that act as toggles for changing the lighting
		bool ambientToggle = false;
		bool specularToggle = false;
		bool noLightingToggle = false;
		bool ambient_And_Specular_Toggle = false;
		bool custom_Shader_Toggle = false;

		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		
		//set More uniform variables for lighting toggles
		shader->SetUniform("u_AmbientToggle", (int)ambientToggle);
		shader->SetUniform("u_SpecularToggle", (int)specularToggle);
		shader->SetUniform("u_LightingOff", (int)noLightingToggle);
		shader->SetUniform("u_AmbientAndSpecToggle", (int)ambient_And_Specular_Toggle);
		shader->SetUniform("u_CustomShaderToggle", (int)custom_Shader_Toggle);
		
		// We'll add some ImGui controls to control our shader
		BackendHandler::imGuiCallbacks.push_back([&]() {
			
			if (ImGui::Checkbox("No Lighting", &noLightingToggle))
			{
				noLightingToggle = true;
				ambientToggle = false;
				specularToggle = false;
				ambient_And_Specular_Toggle = false;
				custom_Shader_Toggle = false;
			
			}
			
			if (ImGui::Checkbox("Ambient", &ambientToggle))
			{
				noLightingToggle = false;
				ambientToggle = true;
				specularToggle = false;
				ambient_And_Specular_Toggle = false;
				custom_Shader_Toggle = false;
		
			}

			if (ImGui::Checkbox("Specular", &specularToggle))
			{
				noLightingToggle = false;
				ambientToggle = false;
				specularToggle = true;
				ambient_And_Specular_Toggle = false;
				custom_Shader_Toggle = false;

			}

			if (ImGui::Checkbox("Ambient + Specular + Diffuse", &ambient_And_Specular_Toggle))
			{
				noLightingToggle = false;
				ambientToggle = false;
				specularToggle = false;
				ambient_And_Specular_Toggle = true;
				custom_Shader_Toggle = false;
			}

			if (ImGui::Checkbox("Ambient + Specular + Toon Shading", &custom_Shader_Toggle))
			{
				noLightingToggle = false;
				ambientToggle = false;
				specularToggle = false;
				ambient_And_Specular_Toggle = false;
				custom_Shader_Toggle = true;
			}

			//Re-set the unifrom variables in the shader after Imgui toggles
			shader->SetUniform("u_AmbientToggle", (int)ambientToggle);
			shader->SetUniform("u_SpecularToggle", (int)specularToggle);
			shader->SetUniform("u_LightingOff", (int)noLightingToggle);
			shader->SetUniform("u_AmbientAndSpecToggle", (int)ambient_And_Specular_Toggle);
			shader->SetUniform("u_CustomShaderToggle", (int)custom_Shader_Toggle);


			if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}

		//	auto name = controllables[selectedVao].get<GameObjectTag>().Name;
		//	ImGui::Text(name.c_str());
			/*auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
			ImGui::Checkbox("Relative Rotation", &behaviour->Relative);*/

			ImGui::Text("Q/E -> Yaw\nLeft/Right -> Roll\nUp/Down -> Pitch\nY -> Toggle Mode");
		
			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			});

		

		#pragma endregion 

		// GL states
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New 

		#pragma region TEXTURE LOADING

		// Load some textures from files
		Texture2D::sptr diffuse = Texture2D::LoadFromFile("images/Stone_001_Diffuse.png");
		Texture2D::sptr diffuse2 = Texture2D::LoadFromFile("images/box.bmp");
		Texture2D::sptr specular = Texture2D::LoadFromFile("images/Stone_001_Specular.png");
		Texture2D::sptr reflectivity = Texture2D::LoadFromFile("images/box-reflections.bmp");
		//Material Applied to plane
		Texture2D::sptr metalDiffuse = Texture2D::LoadFromFile("images/Metal.jpg");
		//Material applied to test tube
		Texture2D::sptr goldDiffuse = Texture2D::LoadFromFile("images/Gold_3.jpg");

#pragma region Box texture
	
		//Loaded in new textures
		Texture2DData::sptr boxDiffuseMap = Texture2DData::LoadFromFile("images/BoxTexture.png");
		//Create New texture to be applied to Box.obj
		Texture2D::sptr boxDiffuse = Texture2D::Create();
		boxDiffuse->LoadData(boxDiffuseMap);
		//Create empty texture
		Texture2DDescription boxDesc = Texture2DDescription();
		boxDesc.Width = 1;
		boxDesc.Height = 1;
		boxDesc.Format = InternalFormat::RGB8;
		Texture2D::sptr boxTexture = Texture2D::Create(boxDesc);


#pragma endregion

#pragma region Chicken texture
		//Loaded in new textures
		Texture2DData::sptr drumstickDiffuseMap = Texture2DData::LoadFromFile("images/Drumstick UV's.png");
		Texture2DData::sptr drumstickSpecularMap = Texture2DData::LoadFromFile("images/Drumstick UV's.png");
		//Create New texture to be applied to Chicken.obj
		Texture2D::sptr drumstickDiffuse = Texture2D::Create();
		drumstickDiffuse->LoadData(drumstickDiffuseMap);
		Texture2D::sptr drumstickSpecular = Texture2D::Create();
		drumstickSpecular->LoadData(drumstickSpecularMap);

		//Create empty texture
		Texture2DDescription drumStickDesc = Texture2DDescription();
		drumStickDesc.Width = 1;
		drumStickDesc.Height = 1;
		drumStickDesc.Format = InternalFormat::RGB8;
		Texture2D::sptr drumStickTexture = Texture2D::Create(drumStickDesc);


#pragma endregion

#pragma region Door Texture
		//Loaded in new textures
		Texture2DData::sptr doorDiffuseMap = Texture2DData::LoadFromFile("images/DoorTexture.png");
		//Create New texture to be applied to Door.obj
		Texture2D::sptr doorDiffuse = Texture2D::Create();
		doorDiffuse->LoadData(doorDiffuseMap);
		//Create empty texture
		Texture2DDescription doorDesc = Texture2DDescription();
		doorDesc.Width = 1;
		doorDesc.Height = 1;
		doorDesc.Format = InternalFormat::RGB8;
		Texture2D::sptr doorTexture = Texture2D::Create(doorDesc);

#pragma endregion

#pragma region Robot Texture
		
		//Loaded in new textures
		Texture2DData::sptr robotDiffuseMap = Texture2DData::LoadFromFile("images/Robot Texture_3.jpg");
		Texture2DData::sptr robotSpecularMap = Texture2DData::LoadFromFile("images/Robot Texture_2.png");
		//Create New texture to be applied to Gun_Bot.obj
		Texture2D::sptr robotDiffuse = Texture2D::Create();
		robotDiffuse->LoadData(robotDiffuseMap);
		//Create empty texture
		Texture2DDescription robotDesc = Texture2DDescription();
		robotDesc.Width = 1;
		robotDesc.Height = 1;
		robotDesc.Format = InternalFormat::RGB8;
		Texture2D::sptr robotTexture = Texture2D::Create(robotDesc);



#pragma endregion


		// Load the cube map
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/sample.jpg");
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ocean.jpg"); 

		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();  
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();

		#pragma endregion

		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create a scene, and set it to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroup =
			scene->Registry().group<RendererComponent>(entt::get_t<Transform>());

		// Create a material and set some properties for it
		ShaderMaterial::sptr material0 = ShaderMaterial::Create();  
		material0->Shader = shader;
		material0->Set("s_Diffuse", diffuse);
		material0->Set("s_Diffuse2", diffuse2);
		material0->Set("s_Specular", specular);
		material0->Set("u_Shininess", 8.0f);
		material0->Set("u_TextureMix", 0.5f); 

		// Load a second material for our reflective material!
		Shader::sptr reflectiveShader = Shader::Create();
		reflectiveShader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		reflectiveShader->LoadShaderPartFromFile("shaders/frag_reflection.frag.glsl", GL_FRAGMENT_SHADER);
		reflectiveShader->Link();

		Shader::sptr reflective = Shader::Create();
		reflective->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		reflective->LoadShaderPartFromFile("shaders/frag_blinn_phong_reflection.glsl", GL_FRAGMENT_SHADER);
		reflective->Link();
	
		// 
		ShaderMaterial::sptr material1 = ShaderMaterial::Create(); 
		material1->Shader = reflective;
		material1->Set("s_Diffuse", diffuse);
		material1->Set("s_Diffuse2", diffuse2);
		material1->Set("s_Specular", specular);
		material1->Set("s_Reflectivity", reflectivity); 
		material1->Set("s_Environment", environmentMap); 
		material1->Set("u_LightPos", lightPos);
		material1->Set("u_LightCol", lightCol);
		material1->Set("u_AmbientLightStrength", lightAmbientPow); 
		material1->Set("u_SpecularLightStrength", lightSpecularPow); 
		material1->Set("u_AmbientCol", ambientCol);
		material1->Set("u_AmbientStrength", ambientPow);
		material1->Set("u_LightAttenuationConstant", 1.0f);
		material1->Set("u_LightAttenuationLinear", lightLinearFalloff);
		material1->Set("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		material1->Set("u_Shininess", 8.0f);
		material1->Set("u_TextureMix", 0.5f);
		material1->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
		
		ShaderMaterial::sptr reflectiveMat = ShaderMaterial::Create();
		reflectiveMat->Shader = reflectiveShader;
		reflectiveMat->Set("s_Environment", environmentMap);
		reflectiveMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));

		//Create new mateirals that stores the texture for the box obj
		ShaderMaterial::sptr material2 = ShaderMaterial::Create();
		material2->Shader = shader;
		material2->Set("s_Diffuse", boxDiffuse);
		material2->Set("s_Specular", boxDiffuse);
		material2->Set("u_Shininess", 8.0f);
		material2->Set("u_TextureMix", 0.5f);

		//Create new mateirals that stores the texture for the chicken obj
		ShaderMaterial::sptr material3 = ShaderMaterial::Create();
		material3->Shader = shader;
		material3->Set("s_Diffuse", drumstickDiffuse);
		material3->Set("s_Specular", drumstickSpecular);
		material3->Set("u_Shininess", 10.0f);
		material3->Set("u_TextureMix", 0.5f);

		//Create new mateirals that stores the texture for the door obj
		ShaderMaterial::sptr material4 = ShaderMaterial::Create();
		material4->Shader = shader;
		material4->Set("s_Diffuse", doorDiffuse);
		material4->Set("s_Specular", doorDiffuse);
		material4->Set("u_Shininess", 8.0f);
		material4->Set("u_TextureMix", 0.5f);

		//Create new mateirals that stores the texture for the box obj
		ShaderMaterial::sptr material5 = ShaderMaterial::Create();
		material5->Shader = shader;
		material5->Set("s_Diffuse", metalDiffuse);
		material5->Set("s_Specular", metalDiffuse);
		material5->Set("u_Shininess", 8.0f);
		material5->Set("u_TextureMix", 0.5f);

		//Create new mateirals that stores the texture for the box obj
		ShaderMaterial::sptr material6 = ShaderMaterial::Create();
		material6->Shader = shader;
		material6->Set("s_Diffuse", robotDiffuse);
		material6->Set("s_Specular", robotDiffuse);
		material6->Set("u_Shininess", 8.0f);
		material6->Set("u_TextureMix", 0.5f);

		//Create new mateirals that stores the texture for the box obj
		ShaderMaterial::sptr material7 = ShaderMaterial::Create();
		material7->Shader = shader;
		material7->Set("s_Diffuse", goldDiffuse);
		material7->Set("s_Specular", goldDiffuse);
		material7->Set("u_Shininess", 8.0f);
		material7->Set("u_TextureMix", 0.5f);
		

	/*	GameObject sceneObj = scene->CreateEntity("scene_geo"); 
		{
			VertexArrayObject::sptr sceneVao = NotObjLoader::LoadFromFile("Sample.notobj");
			sceneObj.emplace<RendererComponent>().SetMesh(sceneVao).SetMaterial(material1);
			sceneObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
		}*/

		GameObject obj2 = scene->CreateEntity("Box");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Box.obj");
			obj2.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material2);
			obj2.get<Transform>().SetLocalPosition(-4.0f, -0.5f, -0.4f);
			obj2.get<Transform>().SetLocalScale(0.1f, 0.1f, 0.1f);
			obj2.get<Transform>().SetLocalRotation(0.0f, 90.0f, 45.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj2);
		}

		GameObject obj13 = scene->CreateEntity("Box");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Box.obj");
			obj13.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material2);
			obj13.get<Transform>().SetLocalPosition(-4.0f, -0.5f, 0.3f);
			obj13.get<Transform>().SetLocalScale(0.1f, 0.1f, 0.1f);
			obj13.get<Transform>().SetLocalRotation(0.0f, 90.0f, 45.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj2);
		}

		GameObject obj14 = scene->CreateEntity("Box");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Box.obj");
			obj14.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material2);
			obj14.get<Transform>().SetLocalPosition(-3.3f, -1.0f, -0.4f);
			obj14.get<Transform>().SetLocalScale(0.1f, 0.1f, 0.1f);
			obj14.get<Transform>().SetLocalRotation(0.0f, 90.0f, 0.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj14);
		}

		GameObject obj15 = scene->CreateEntity("Box");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Box.obj");
			obj15.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material2);
			obj15.get<Transform>().SetLocalPosition(-3.3f, -1.0f, 0.3f);
			obj15.get<Transform>().SetLocalScale(0.1f, 0.1f, 0.1f);
			obj15.get<Transform>().SetLocalRotation(0.0f, 90.0f, 0.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj14);
		}

		
		GameObject obj3 = scene->CreateEntity("Test Tube");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestTube.obj");
			obj3.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material7);
			obj3.get<Transform>().SetLocalPosition(4.0f, -2.5f, -0.8f);
			obj3.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			obj3.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj3);
		}

		GameObject obj4 = scene->CreateEntity("Chicken Model");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Drumstick Walk Frame 1.obj");
			obj4.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material3);
			obj4.get<Transform>().SetLocalPosition(1.3f, 1.0f, -0.8f);
			obj4.get<Transform>().SetLocalScale(0.2f, 0.2f, 0.2f);
			obj4.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			
			// Bind returns a smart pointer to the behaviour that was added
			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj4);
			// Set up a path for the object to follow
			pathing->Points.push_back({ 1.3f, -2.0f, -0.8f });
			pathing->Points.push_back({ -1.3f, -2.0f, -0.8f });
			pathing->Points.push_back({ -1.3f, 2.0f, -0.8f });
			pathing->Points.push_back({ 1.3f, 2.0f, -0.8f });
			pathing->Speed = 3.0f;
		}
		
		GameObject obj5 = scene->CreateEntity("Door Model");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Door-FIXED.obj");
			obj5.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material4);
			obj5.get<Transform>().SetLocalPosition(1.5f, -4.5f, 2.5f);
			obj5.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			obj5.get<Transform>().SetLocalRotation(0.0f, 90.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj5);
		}

		GameObject obj6 = scene->CreateEntity("Plane");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Simple Plane.obj");
			obj6.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material5);
			obj6.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			obj6.get<Transform>().SetLocalScale(0.9f, 0.9f, 0.9f);
			obj6.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj6);
		}

		GameObject obj8 = scene->CreateEntity("Test Tube");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestTube.obj");
			obj8.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material7);
			obj8.get<Transform>().SetLocalPosition(4.0f, 1.0f, -0.8f);
			obj8.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			obj8.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj8);
		}

		GameObject obj9 = scene->CreateEntity("Test Tube");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestTube.obj");
			obj9.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material7);
			obj9.get<Transform>().SetLocalPosition(4.0f, 4.0f, -0.8f);
			obj9.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			obj9.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj9);
		}

		GameObject obj10 = scene->CreateEntity("Test Tube");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestTube.obj");
			obj10.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material7);
			obj10.get<Transform>().SetLocalPosition(-4.0f, -2.5f, -0.8f);
			obj10.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			obj10.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj10);
		}

		GameObject obj11 = scene->CreateEntity("Test Tube");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestTube.obj");
			obj11.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material7);
			obj11.get<Transform>().SetLocalPosition(-4.0f, 1.0f, -0.8f);
			obj11.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			obj11.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj11);
		}

		GameObject obj12 = scene->CreateEntity("Test Tube");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestTube.obj");
			obj12.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material7);
			obj12.get<Transform>().SetLocalPosition(-4.0f, 4.0f, -0.8f);
			obj12.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			obj12.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj12);
		}

		GameObject obj16 = scene->CreateEntity("Robot");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Gun_Bot.obj");
			obj16.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material6);
			obj16.get<Transform>().SetLocalPosition(1.3f, 3.0f, 0.0f);
			obj16.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			obj16.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			//obj16.get<Transform>().SetParent(obj4);
	

			// Bind returns a smart pointer to the behaviour that was added
			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj16);
			// Set up a path for the object to follow
			pathing->Points.push_back({ 1.3f, -2.0f, -0.8f });
			pathing->Points.push_back({ -1.3f, -2.0f, -0.8f });
			pathing->Points.push_back({ -1.3f, 2.0f, -0.8f });
			pathing->Points.push_back({ 1.3f, 2.0f, -0.8f });
			pathing->Speed = 3.0f;

			
		}

		//GameObject obj5 = scene->CreateEntity("cube");
		//{
		//	MeshBuilder<VertexPosNormTexCol> builder = MeshBuilder<VertexPosNormTexCol>();
		//	MeshFactory::AddCube(builder, glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f));
		//	VertexArrayObject::sptr vao = builder.Bake();
		//	
		//	obj5.emplace<RendererComponent>().SetMesh(vao).SetMaterial(reflectiveMat);
		//	obj5.get<Transform>().SetLocalPosition(-4.0f, 0.0f, 2.0f);
		//	BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(obj5);
		//}

		//GameObject obj4 = scene->CreateEntity("moving_box");
		//{
		//	// Build a mesh
		//	MeshBuilder<VertexPosNormTexCol> builder = MeshBuilder<VertexPosNormTexCol>();
		//	MeshFactory::AddCube(builder, glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
		//	VertexArrayObject::sptr vao = builder.Bake();
		//	
		//	obj4.emplace<RendererComponent>().SetMesh(vao).SetMaterial(material0);
		//	obj4.get<Transform>().SetLocalPosition(-2.0f, 0.0f, 1.0f);

		//	// Bind returns a smart pointer to the behaviour that was added
		//	auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj4);
		//	// Set up a path for the object to follow
		//	pathing->Points.push_back({ -4.0f, -4.0f, 0.0f });
		//	pathing->Points.push_back({ 4.0f, -4.0f, 0.0f });
		//	pathing->Points.push_back({ 4.0f,  4.0f, 0.0f });
		//	pathing->Points.push_back({ -4.0f,  4.0f, 0.0f });
		//	pathing->Speed = 2.0f;
		//}

		//GameObject obj6 = scene->CreateEntity("following_monkey");
		//{
		//	VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/monkey.obj");
		//	obj6.emplace<RendererComponent>().SetMesh(vao).SetMaterial(reflectiveMat);
		//	obj6.get<Transform>().SetLocalPosition(0.0f, 0.0f, 3.0f);
		//	obj6.get<Transform>().SetParent(obj4);
		//	
		//	auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(obj6);
		//	// Set up a path for the object to follow
		//	pathing->Points.push_back({ 0.0f, 0.0f, 1.0f });
		//	pathing->Points.push_back({ 0.0f, 0.0f, 3.0f });
		//	pathing->Speed = 2.0f;
		//}
		
		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 3, 3).LookAt(glm::vec3(0, 0, 0));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::Bind<CameraControlBehaviour>(cameraObject);
		}

		#pragma endregion 
		//////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////// SKYBOX ///////////////////////////////////////////////
		{
			// Load our shaders
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();

			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;  
			skyboxMat->Set("s_Environment", environmentMap);
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
			skyboxMat->RenderLayer = 100;

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
			GameObject skyboxObj = scene->CreateEntity("skybox");  
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);
		}
		////////////////////////////////////////////////////////////////////////////////////////


		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
			// how this is implemented. Note that the ampersand here is capturing the variables within
			// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
			// use std::bind
			keyToggles.emplace_back(GLFW_KEY_T, [&]() { cameraObject.get<Camera>().ToggleOrtho(); });

			//controllables.push_back(obj2);
			//controllables.push_back(obj3);

			keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao++;
				if (selectedVao >= controllables.size())
					selectedVao = 0;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});
			keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao--;
				if (selectedVao < 0)
					selectedVao = controllables.size() - 1;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});

			keyToggles.emplace_back(GLFW_KEY_Y, [&]() {
				auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
				behaviour->Relative = !behaviour->Relative;
				});
		}

		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		///// Game loop /////
		while (!glfwWindowShouldClose(BackendHandler::window)) {
			glfwPollEvents();

			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;


			//Change the Chicken Rotation when he reaches reach a certain point
			if (obj4.get<Transform>().GetLocalPosition().y <= -1.90)
			{
				obj4.get<Transform>().SetLocalRotation(90.0f, 0.0f, 270.0f);

			}

			if (obj4.get<Transform>().GetLocalPosition().x <= -1.2)
			{
				obj4.get<Transform>().SetLocalRotation(90.0f, 0.0f, 180.0f);
			}

			if (obj4.get<Transform>().GetLocalPosition().y >= 1.90)
			{
				obj4.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);

			}

			if (obj4.get<Transform>().GetLocalPosition().x >= 1.2)
			{
				obj4.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			}


			//Change the Robot's Rotation when it reaches a certain point
			if (obj16.get<Transform>().GetLocalPosition().y <= -1.90)
			{
				obj16.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);

			}

			if (obj16.get<Transform>().GetLocalPosition().x <= -1.2)
			{
				obj16.get<Transform>().SetLocalRotation(90.0f, 0.0f, 270.0f);
			}

			if (obj16.get<Transform>().GetLocalPosition().y >= 1.90)
			{
				obj16.get<Transform>().SetLocalRotation(90.0f, 0.0f, 180.0f);

			}

			if (obj16.get<Transform>().GetLocalPosition().x >= 1.2)
			{
				obj16.get<Transform>().SetLocalRotation(90.0f, 0.0f, 90.0f);
			}



			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(BackendHandler::window);
				}
			}

			// Iterate over all the behaviour binding components
			scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
				// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
				for (const auto& behaviour : binding.Behaviours) {
					if (behaviour->Enabled) {
						behaviour->Update(entt::handle(scene->Registry(), entity));
					}
				}
			});

			// Clear the screen
			glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update all world matrices for this frame
			scene->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
				t.UpdateWorldMatrix();
			});
			
			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;
						
			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
				// Sort by render layer first, higher numbers get drawn last
				if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
				if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

				// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
				if (l.Material->Shader < r.Material->Shader) return true;
				if (l.Material->Shader > r.Material->Shader) return false;

				// Sort by material pointer last (so we can minimize switching between materials)
				if (l.Material < r.Material) return true;
				if (l.Material > r.Material) return false;
				
				return false;
			});

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			// Iterate over the render group components and draw them
			renderGroup.each( [&](entt::entity e, RendererComponent& renderer, Transform& transform) {
				// If the shader has changed, set up it's uniforms
				if (current != renderer.Material->Shader) {
					current = renderer.Material->Shader;
					current->Bind();
					BackendHandler::SetupShaderForFrame(current, view, projection);
				}
				// If the material has changed, apply it
				if (currentMat != renderer.Material) {
					currentMat = renderer.Material;
					currentMat->Apply();
				}
				// Render the mesh
				BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
			});

			// Draw our ImGui content
			BackendHandler::RenderImGui();

			scene->Poll();
			glfwSwapBuffers(BackendHandler::window);
			time.LastFrame = time.CurrentFrame;
		}

		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;
		BackendHandler::ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}