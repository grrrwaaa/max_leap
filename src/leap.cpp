/**
	@file
	leap - a max object
 
	Inspired by and derived from https://github.com/akamatsu/aka.leapmotion/ (http://akamatsu.org/aka/max/objects/) (Masayuki Akamatsu), which is shared under Creative Commons Attribution 3.0 Unported License.
 
	Updated to V2 SDK and slightly extended by Graham Wakefield, 2015
 
 */

#include "Leap.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "ext.h"
#include "ext_obex.h"
    
#include "jit.common.h"
#include "jit.gl.h"
#ifdef __cplusplus
}
#endif

#include <new>

t_class *leap_class;
static t_symbol * ps_frame_start;
static t_symbol * ps_frame_end;
static t_symbol * ps_frame;
static t_symbol * ps_hand;
static t_symbol * ps_finger;
static t_symbol * ps_palm;
static t_symbol * ps_ball;
static t_symbol * ps_connected;
static t_symbol * ps_fps;
static t_symbol * ps_probability;

class t_leap {
	
	struct SerializedFrame {
	public:
		int32_t length;
		unsigned char data[16380];	// i.e. total frame size is 16384
	};

	class LeapListener : public Leap::Listener {
	public:
	
		t_leap * owner;
		
		virtual void onDeviceChange(const Leap::Controller &) {}
		
		virtual void onFocusGained(const Leap::Controller &) {}
		virtual void onFocusLost(const Leap::Controller &) {}
		
		virtual void onServiceConnect(const Leap::Controller &) {}
		virtual void onServiceDisconnect(const Leap::Controller &) {}
	
		virtual void onConnect(const Leap::Controller&) {
			owner->configure();
		}
		
		virtual void onDisconnect(const Leap::Controller &) {}
		
		// do nothing -- we're going to poll with bang() instead:
		virtual void onFrame(const Leap::Controller&) {}
	};

public:
    t_object	ob;			// the object itself (must be first)
    
	int			unique;		// only output new data
	int			allframes;	// output all frames between each poll (rather than just the latest frame)
	int			serialize;	// output serialized frames
	int 		images;		// output the raw images
	int			motion_tracking;
	int			hmd;		// optimize for LeapVR HMD mount
	int			background;	// capture data even when Max has lost focus
	int			aka;		// output in a form compatible with aka.leapmotion
	
	void *		outlet_frame;
	void *		outlet_image[2];
	void *		outlet_tracking;
	void *		outlet_msg;
	
	// matrices for the IR images:
	void *		image_wrappers[2];
	void *		image_mats[2];
	int			image_width, image_height;
	
	Leap::Controller controller;
	LeapListener listener;
	Leap::Frame lastFrame;
	int64_t		lastFrameID;
	
	t_leap(int index = 0) {
		outlet_msg = outlet_new(&ob, 0);
		outlet_tracking = outlet_new(&ob, 0);
		outlet_image[0] = outlet_new(&ob, "jit_matrix");
		outlet_image[1] = outlet_new(&ob, "jit_matrix");
        outlet_frame = outlet_new(&ob, 0);

		// attrs:
		unique = 0;
		allframes = 0;
		images = 1;
		aka = 0;
		serialize = 0;
		motion_tracking = 0;
		hmd = 0;
		background = 1;
		
		image_width = 640;
		image_height = 240;
		for (int i=0; i<2; i++) {
			// create matrix:
			image_wrappers[i] = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
			image_mats[i] = configureMatrix2D(image_wrappers[i], 1, _jit_sym_char, image_width, image_height);
		}
		
		// internal:
		lastFrameID = 0;
		listener.owner = this;
        controller.addListener(listener);
    }
    
    ~t_leap() {
		for (int i=0; i<2; i++) {
			object_release((t_object *)image_wrappers[i]);
		}
    }
	
	void * configureMatrix2D(void * mat_wrapper, long planecount, t_symbol * type, long w, long h) {
		void * mat = jit_object_method(mat_wrapper, _jit_sym_getmatrix);
		t_jit_matrix_info info;
		jit_matrix_info_default(&info);
		info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
		info.planecount = planecount;
		info.type = type;
		info.dimcount = 2;
		info.dim[0] = w;
		info.dim[1] = h;
		jit_object_method(mat, _jit_sym_setinfo_ex, &info);
		return mat;
	}
	
    void configure() {
        int flag = Leap::Controller::POLICY_DEFAULT;
		if (images)		flag |= Leap::Controller::POLICY_IMAGES;
		if (hmd)		flag |= Leap::Controller::POLICY_OPTIMIZE_HMD;
		if (background) flag |= Leap::Controller::POLICY_BACKGROUND_FRAMES;
		
		controller.setPolicyFlags((Leap::Controller::PolicyFlag)flag);
		
		// set connected status:
		//outlet_anything(owner->outlet_frame, ps_connected, 1, );
		
		// controller.enableGesture(Gesture::TYPE_SWIPE);
    }
	
	void serializeAndOutput(const Leap::Frame& frame) {
		t_atom a[1];
		std::string s = frame.serialize();
		size_t len = s.length();
		SerializedFrame * mat_ptr = 0;
		if (len <= sizeof(SerializedFrame)-4) {
			
			// dump via a jit_matrix 1 char len
			
			// export this distortion mesh to Jitter
			t_jit_matrix_info info;
			
			// create matrix:
			void * mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
			void * mat = jit_object_method(mat_wrapper, _jit_sym_getmatrix);
			
			// configure matrix:
			jit_matrix_info_default(&info);
			info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
			info.planecount = 1;
			info.type = gensym("char");
			info.dimcount = 1;
			info.dim[0] = 16384;
			jit_object_method(mat, _jit_sym_setinfo_ex, &info);
			
			// copy data:
			jit_object_method(mat, _jit_sym_getdata, &mat_ptr);
			if (mat_ptr) {
				mat_ptr->length = len;
				memcpy(mat_ptr->data, s.data(), len);
				
				// output matrix:
				atom_setsym(a, jit_attr_getsym(mat_wrapper, _jit_sym_name));
				outlet_anything(outlet_msg, gensym("serialized_frame"), 1, a);
			}
			
			// done with matrix:
			object_release((t_object *)mat_wrapper);
		}
	}
	
	void processNextFrame(const Leap::Frame& frame, int serialize=0) {
		if (!frame.isValid()) return;
		
		t_atom a[1];
		
		// serialize:
		if (serialize) serializeAndOutput(frame);
		
		int64_t frame_id = frame.id();
		
		// TODO: Following entities across frames
		// perhaps have an attribute for an ID to track?
		// hand = frame.hand(handID);
		// finger = frame.finger(fingerID) etc.
		
		const Leap::HandList hands = frame.hands();
		const Leap::PointableList pointables = frame.pointables();
		const Leap::FingerList fingers = frame.fingers();
		const Leap::ToolList tools = frame.tools();
		
		const size_t numHands = hands.count();
		
		t_atom frame_data[4];
		atom_setlong(frame_data, frame_id);
		atom_setlong(frame_data+1, frame.timestamp());
		atom_setlong(frame_data+2, numHands);
		// front-most hand ID:
		atom_setlong(frame_data+3, frame.hands().frontmost().id());
		outlet_anything(outlet_frame, ps_frame, 4, frame_data);
		
		// motion tracking
		// motion tracking data is preceded by a probability vector (Rotate, Scale, Translate)
		if (motion_tracking) {
			t_atom transform[4];
			Leap::Vector vec;
			
			atom_setfloat(transform+0, frame.rotationProbability(lastFrame));
			atom_setfloat(transform+1, frame.scaleProbability(lastFrame));
			atom_setfloat(transform+2, frame.translationProbability(lastFrame));
			outlet_anything(outlet_tracking, ps_probability, 3, frame_data);
			
			vec = frame.rotationAxis(lastFrame);
			atom_setfloat(transform, frame.rotationAngle(lastFrame));
			atom_setfloat(transform+1, vec.x);
			atom_setfloat(transform+2, vec.y);
			atom_setfloat(transform+3, vec.z);
			outlet_anything(outlet_tracking, _jit_sym_rotate, 4, frame_data);
			
			atom_setfloat(transform, frame.scaleFactor(lastFrame));
			outlet_anything(outlet_tracking, _jit_sym_scale, 1, frame_data);
			
			vec = frame.translation(lastFrame);
			atom_setfloat(transform+0, vec.x);
			atom_setfloat(transform+1, vec.y);
			atom_setfloat(transform+2, vec.z);
			outlet_anything(outlet_tracking, _jit_sym_position, 3, frame_data);
		}
		
		for(size_t i = 0; i < numHands; i++){
			// Hand
			const Leap::Hand &hand = hands[i];
			const int32_t hand_id = hand.id();
			const Leap::FingerList &fingers = hand.fingers();
			
			// TODO: v2 additions:
			// handedness
			// digits
			// bone orientation / position
			// grip factors
			// finger extension
			
			bool isRight = hand.isRight();
			
			t_atom hand_data[8];
			atom_setlong(hand_data, hand_id);
			atom_setlong(hand_data+1, frame_id);
			atom_setlong(hand_data+2, isRight);	// handedness: 0 (left) or 1 (right)
			atom_setfloat(hand_data+3, hand.confidence());
			atom_setfloat(hand_data+4, hand.grabStrength()); // open hand (0) to grabbing pose (1)
			atom_setfloat(hand_data+5, hand.pinchStrength()); // open hand (0) to pinching pose (1)
			atom_setfloat(hand_data+6, hand.palmWidth() * 0.001); // in meters
			
			outlet_anything(outlet_frame, ps_hand, 7, hand_data);
			
			// Note: Since the left hand is a mirror of the right hand, the basis matrix will be left-handed for left hands.
			// could flip that with isRight ?
			// hand basis vectors:
			//				Leap::Matrix basis = hand.basis();
			//				Leap::Vector xBasis = basis.xBasis;
			//				Leap::Vector yBasis = basis.yBasis;
			//				Leap::Vector zBasis = basis.zBasis;
			
			
			//				Leap::Arm arm = hand.arm();
			// arm.isValid()
			// arm.wristPosition() arm.elbowPosition()
			// Leap::Vector direction = arm.direction();
			// arm.basis()	// left arm uses left-handed basis, vice versa
			// Leap::Vector armCenter = arm.elbowPosition() + (arm.wristPosition() - arm.elbowPosition()) * .5;
			// Leap::Matrix transform = Leap::Matrix(xBasis, yBasis, zBasis, armCenter);
			
			for(size_t j = 0; j < 5; j++) {
				// Finger
				const Leap::Finger &finger = fingers[j];
				const Leap::Finger::Type fingerType = finger.type(); // 0=THUMB ... 4=PINKY
				const int32_t finger_id = finger.id();
				//const Leap::Ray& tip = finger.tip();
				const Leap::Vector direction = finger.direction();
				const Leap::Vector position = finger.tipPosition();
				const Leap::Vector velocity = finger.tipVelocity();
				const double width = finger.width();
				const double lenght = finger.length();
				const bool isTool = finger.isTool();
				
				// getting individual bones:
				// finger.bone(Bone::Type boneIdx)
				// @see https://developer.leapmotion.com/documentation/cpp/api/Leap.Bone.html#cppclass_leap_1_1_bone
				// bone has basis, center, direction, isValid, length, type, width, etc.
				
				t_atom finger_data[15];
				atom_setlong(finger_data, finger_id);
				atom_setlong(finger_data+1, hand_id);
				atom_setlong(finger_data+2, frame_id);
				atom_setfloat(finger_data+3, position.x);
				atom_setfloat(finger_data+4, position.y);
				atom_setfloat(finger_data+5, position.z);
				atom_setfloat(finger_data+6, direction.x);
				atom_setfloat(finger_data+7, direction.y);
				atom_setfloat(finger_data+8, direction.z);
				atom_setfloat(finger_data+9, velocity.x);
				atom_setfloat(finger_data+10, velocity.y);
				atom_setfloat(finger_data+11, velocity.z);
				atom_setfloat(finger_data+12, width);
				atom_setfloat(finger_data+13, lenght);
				atom_setlong(finger_data+14, isTool);
				outlet_anything(outlet_frame, ps_finger, 15, finger_data);
			}
			
			// Palm
			const Leap::Vector position = hand.palmPosition();
			const Leap::Vector direction = hand.direction();
			
			t_atom palm_data[14];
			atom_setlong(palm_data, hand_id);
			atom_setlong(palm_data+1, frame_id);
			atom_setfloat(palm_data+2, position.x);
			atom_setfloat(palm_data+3, position.y);
			atom_setfloat(palm_data+4, position.z);
			atom_setfloat(palm_data+5, direction.x);
			atom_setfloat(palm_data+6, direction.y);
			atom_setfloat(palm_data+7, direction.z);
			
			// Palm Velocity
			const Leap::Vector velocity = hand.palmVelocity();
			atom_setfloat(palm_data+8, velocity.x);
			atom_setfloat(palm_data+9, velocity.y);
			atom_setfloat(palm_data+10, velocity.z);
			
			// Palm Normal
			const Leap::Vector normal = hand.palmNormal();
			atom_setfloat(palm_data+11, normal.x);
			atom_setfloat(palm_data+12, normal.y);
			atom_setfloat(palm_data+13, normal.z);
			outlet_anything(outlet_frame, ps_palm, 14, palm_data);
			
			// Ball
			//const Leap::Ball* ball = hand.ball();
			//if (ball != nil)
			//{
			const Leap::Vector sphereCenter = hand.sphereCenter();
			const double sphereRadius = hand.sphereRadius();
			
			t_atom ball_data[6];
			atom_setlong(ball_data, hand_id);
			atom_setlong(ball_data+1, frame_id);
			atom_setfloat(ball_data+2, sphereCenter.x);
			atom_setfloat(ball_data+3, sphereCenter.y);
			atom_setfloat(ball_data+4, sphereCenter.z);
			atom_setfloat(ball_data+5, sphereRadius);
			outlet_anything(outlet_frame, ps_ball, 6, ball_data);
			//}
		}
		
		//	frame.tools().count()
		//	frame.gestures().count() << std::endl;
		// frame.interactionBox()
		
		outlet_anything(outlet_frame, ps_frame_end, 0, NULL);
	}
	
	// compatibilty with aka.leapmotion:
	void processNextFrameAKA(const Leap::Frame& frame) {
		t_atom a[1];
		t_atom frame_data[3];
		int64_t frame_id = frame.id();
		
        if(frame.isValid()) {
			outlet_anything(outlet_frame, ps_frame_start, 0, NULL);
			
			const Leap::HandList hands = frame.hands();
			const size_t numHands = hands.count();
			
			atom_setlong(frame_data, frame_id);
			atom_setlong(frame_data+1, frame.timestamp());
			atom_setlong(frame_data+2, numHands);
			outlet_anything(outlet_frame, ps_frame, 3, frame_data);
			
			for(size_t i = 0; i < numHands; i++){
				// Hand
				const Leap::Hand &hand = hands[i];
				const int32_t hand_id = hand.id();
				const Leap::FingerList &fingers = hand.fingers();
				bool isRight = hand.isRight();
				
				t_atom hand_data[3];
				atom_setlong(hand_data, hand_id);
				atom_setlong(hand_data+1, frame_id);
				atom_setlong(hand_data+2, fingers.count());
				outlet_anything(outlet_frame, ps_hand, 3, hand_data);
				
				for(size_t j = 0; j < 5; j++) {
					// Finger
					const Leap::Finger &finger = fingers[j];
					const Leap::Finger::Type fingerType = finger.type(); // 0=THUMB ... 4=PINKY
					const int32_t finger_id = finger.id();
					//const Leap::Ray& tip = finger.tip();
					const Leap::Vector direction = finger.direction();
					const Leap::Vector position = finger.tipPosition();
					const Leap::Vector velocity = finger.tipVelocity();
					const double width = finger.width();
					const double lenght = finger.length();
					const bool isTool = finger.isTool();
					
					t_atom finger_data[15];
					atom_setlong(finger_data, finger_id);
					atom_setlong(finger_data+1, hand_id);
					atom_setlong(finger_data+2, frame_id);
					atom_setfloat(finger_data+3, position.x);
					atom_setfloat(finger_data+4, position.y);
					atom_setfloat(finger_data+5, position.z);
					atom_setfloat(finger_data+6, direction.x);
					atom_setfloat(finger_data+7, direction.y);
					atom_setfloat(finger_data+8, direction.z);
					atom_setfloat(finger_data+9, velocity.x);
					atom_setfloat(finger_data+10, velocity.y);
					atom_setfloat(finger_data+11, velocity.z);
					atom_setfloat(finger_data+12, width);
					atom_setfloat(finger_data+13, lenght);
					atom_setlong(finger_data+14, isTool);
					outlet_anything(outlet_frame, ps_finger, 15, finger_data);
				}
				
				const Leap::Vector position = hand.palmPosition();
				const Leap::Vector direction = hand.direction();
				
				t_atom palm_data[14];
				atom_setlong(palm_data, hand_id);
				atom_setlong(palm_data+1, frame_id);
				atom_setfloat(palm_data+2, position.x);
				atom_setfloat(palm_data+3, position.y);
				atom_setfloat(palm_data+4, position.z);
				atom_setfloat(palm_data+5, direction.x);
				atom_setfloat(palm_data+6, direction.y);
				atom_setfloat(palm_data+7, direction.z);
				
				// Palm Velocity
				const Leap::Vector velocity = hand.palmVelocity();
				
				atom_setfloat(palm_data+8, velocity.x);
				atom_setfloat(palm_data+9, velocity.y);
				atom_setfloat(palm_data+10, velocity.z);
				
				// Palm Normal
				const Leap::Vector normal = hand.palmNormal();
				
				atom_setfloat(palm_data+11, normal.x);
				atom_setfloat(palm_data+12, normal.y);
				atom_setfloat(palm_data+13, normal.z);
				outlet_anything(outlet_frame, ps_palm, 14, palm_data);
				
				const Leap::Vector sphereCenter = hand.sphereCenter();
				const double sphereRadius = hand.sphereRadius();
				
				t_atom ball_data[6];
				atom_setlong(ball_data, hand_id);
				atom_setlong(ball_data+1, frame_id);
				atom_setfloat(ball_data+2, sphereCenter.x);
				atom_setfloat(ball_data+3, sphereCenter.y);
				atom_setfloat(ball_data+4, sphereCenter.z);
				atom_setfloat(ball_data+5, sphereRadius);
				outlet_anything(outlet_frame, ps_ball, 6, ball_data);
			}
			outlet_anything(outlet_frame, ps_frame_end, 0, NULL);
		}
	}
	
	void processImageList(const Leap::ImageList& images) {
		t_atom a[1];
		if (images.count() < 2) return;
		for(int i = 0; i < 2; i++){
			Leap::Image image = images[i];
			if (image.isValid()) {
				
				//int64_t id = image.sequenceId(); // like frame.id(); can be used for unique/allframes etc.
				int idx = image.id();
				
				void * mat_wrapper = image_wrappers[idx];
				void * mat = image_mats[idx];
				
				// std::string toString()
				//A string containing a brief, human readable description of the Image object.
				
				// sanity checks:
				// not sure if this ever happens, but just in case:
				if (image.width() != image_width || image.height() != image_height) {
					image_mats[i] = configureMatrix2D(image_wrappers[i], 1, _jit_sym_char, image_width, image_height);
				}
				if (image.bytesPerPixel() != 1) {
					post("Leap SDK has changed the image format, so the max object will need to be recompiled...");
					return;
				}
				
				// copy into image:
				char * out_bp;
				jit_object_method(mat, _jit_sym_getdata, &out_bp);
				memcpy(out_bp, image.data(), image.bytesPerPixel()*image.width()*image.height());
				
				// output image:
				atom_setsym(a, jit_attr_getsym(mat_wrapper, _jit_sym_name));
				outlet_anything(outlet_image[idx], _jit_sym_jit_matrix, 1, a);
				
				/*
				 see https://developer.leapmotion.com/documentation/cpp/api/Leap.Image.html#cppclass_leap_1_1_image_1a4c6fa722eba7018e148b13677c7ce609
				 */
				// image.distortionHeight(), image.distortionWidth();
				// Since each point on the 64x64 element distortion map has two values in the buffer, the stride is 2 times the size of the grid. (Stride is currently fixed at 2 * 64 = 128).
				//const float* distortion_buffer = image.distortion();
				
				// see https://developer.leapmotion.com/documentation/cpp/api/Leap.Image.html#cppclass_leap_1_1_image_1a362c8bbd9e27224f9c0d0eeb6962ccf5
				// image.rayOffsetX, rayOffsetY() //Used to convert between normalized coordinates in the range [0..1] and the ray slope range [-4..4].
				// image.rectify():  Given a point on the image, rectify() corrects for camera distortion and returns the true direction from the camera to the source of that image point within the Leap Motion field of view.
				// image.warp()
				// Provides the point in the image corresponding to a ray projecting from the camera.
				// warp() is typically not fast enough for realtime distortion correction. For better performance, use a shader program exectued on a GPU.
			}
		}
	}
	
    void bang() {
		t_atom a[1];
		atom_setlong(a, controller.isConnected());
		outlet_anything(outlet_msg, ps_connected, 1, a);
		
		if(!controller.isConnected()) return;
			
		Leap::Frame frame = controller.frame();
		float fps = frame.currentFramesPerSecond();
		atom_setfloat(a, fps);
		outlet_anything(outlet_msg, ps_fps, 1, a);
		
		int64_t currentID = frame.id();
		if ((!unique) || currentID > lastFrameID) {		// is this frame new?
			if (allframes) {
				// output all pending frames:
				for (int history = 0; history < currentID - lastFrameID; history++) {
					frame = controller.frame(history);
					if (images) {
						// get most recent images:
						processImageList(frame.images());
					}				
					if (aka) {
						processNextFrameAKA(frame);
					} else {
						processNextFrame(frame, serialize);
					}
				}
			} else {
				if (images) {
					// get most recent images:
					processImageList(controller.images());
				}				
				// The latest frame only
				if (aka) {
					processNextFrameAKA(frame);
				} else {
					processNextFrame(frame, serialize);
				}
			}
			lastFrameID = currentID;
		}
		lastFrame = frame;
    }
	
	void jit_matrix(t_symbol * name) {
		int len;
		t_jit_matrix_info in_info;
		long in_savelock;
		SerializedFrame * in_bp;
		t_jit_err err = 0;
		Leap::Frame frame;
		
		// get matrix from name:
		void * in_mat = jit_object_findregistered(name);
		if (!in_mat) {
			object_error(&ob, "failed to acquire matrix");
			err = JIT_ERR_INVALID_INPUT;
			goto out;
		}
		
		// lock it:
		in_savelock = (long)jit_object_method(in_mat, _jit_sym_lock, 1);
		
		// first ensure the type is correct:
		jit_object_method(in_mat, _jit_sym_getinfo, &in_info);
		jit_object_method(in_mat, _jit_sym_getdata, &in_bp);
		if (!in_bp) {
			err = JIT_ERR_INVALID_INPUT;
			goto unlock;
		}
		if (in_info.planecount != 1) {
			err = JIT_ERR_MISMATCH_PLANE;
			goto unlock;
		}
		if (in_info.type != _jit_sym_char) {
			err = JIT_ERR_MISMATCH_TYPE;
			goto unlock;
		}
		if (in_info.dimcount != 1) {
			err = JIT_ERR_MISMATCH_DIM;
			goto unlock;
		}
		
		frame.deserialize(in_bp->data, in_bp->length);
		if (aka) {
			processNextFrameAKA(frame);
		} else {
			processNextFrame(frame, 0);
		}
		
	unlock:
		// restore matrix lock state:
		jit_object_method(in_mat, _jit_sym_lock, in_savelock);
	out:
		if (err) {
			jit_error_code(&ob, err);
		}
	}
};

//t_max_err leap_notify(t_leap *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
//    t_symbol *attrname;
//    if (msg == _sym_attr_modified) {       // check notification type
//        attrname = (t_symbol *)object_method((t_object *)data, _sym_getname);
//        object_post((t_object *)x, "changed attr name is %s", attrname->s_name);
//    } else {
//        object_post((t_object *)x, "notify %s (self %d)", msg->s_name, sender == x);
//    }
//    return 0;
//}

void leap_bang(t_leap * x) {
    x->bang();
}

void leap_jit_matrix(t_leap *x, t_symbol * s) {
	x->jit_matrix(s);
}

void leap_doconfigure(t_leap *x) {
    x->configure();
}

void leap_configure(t_leap *x) {
    defer_low(x, (method)leap_doconfigure, 0, 0, 0);
}

t_max_err leap_images_set(t_leap *x, t_object *attr, long argc, t_atom *argv) {
    x->images = atom_getlong(argv);
    leap_configure(x);
    return 0;
}

t_max_err leap_hmd_set(t_leap *x, t_object *attr, long argc, t_atom *argv) {
	x->hmd = atom_getlong(argv);
	leap_configure(x);
	return 0;
}

void leap_assist(t_leap *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) { // inlet
        if (a == 0) {
            sprintf(s, "bang to report frame data");
        } else {
            sprintf(s, "I am inlet %ld", a);
        }
    } else {	// outlet
        if (a == 0) {
            sprintf(s, "frame data (messages)");
        } else if (a == 1) {
            sprintf(s, "image (left)");
        } else if (a == 2) {
            sprintf(s, "image (right)");
        } else if (a == 3) {
            sprintf(s, "motion tracking data (messages)");
//        } else if (a == 4) {
//            sprintf(s, "HMD left eye mesh (jit_matrix)");
//        } else if (a == 5) {
//            sprintf(s, "HMD right eye properties (messages)");
//        } else if (a == 6) {
//            sprintf(s, "HMD right eye mesh (jit_matrix)");
//        } else if (a == 7) {
//            sprintf(s, "HMD properties (messages)");
        } else {
            sprintf(s, "other messages");
			//sprintf(s, "I am outlet %ld", a);
        }
    }
}


void leap_free(t_leap *x) {
    x->~t_leap();
    max_jit_object_free(x);
}

void *leap_new(t_symbol *s, long argc, t_atom *argv)
{
    t_leap *x = NULL;
    if ((x = (t_leap *)object_alloc(leap_class))) {
		// initialize in-place:
        x = new (x) t_leap();
        // apply attrs:
        attr_args_process(x, argc, argv);
    }
    return (x);
}

int C74_EXPORT main(void) {	
    t_class *maxclass;

#ifdef WIN_VERSION
	// Rather annoyingly, leap don't provide static libraries, only a Leap.dll
	// which should sit next to Max.exe -- not very user friendly for Max.
	// So instead we delay load the library from next to the leap.mxe
	{
		char path[MAX_PATH];
		char mxepath[MAX_PATH];
		char dllpath[MAX_PATH];
		const char * name = "Leap.dll";
		// get leap.mxe as a HMODULE:
		HMODULE hm = NULL;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR) &leap_class, &hm)) {
			error("critical error loading leap.mxe: GetModuleHandle returned %d\n", (int)GetLastError());
			return 0;
		}
		// get its full path, the containing directory, and then munge the desired dllpath from it:
		GetModuleFileNameA(hm, path, sizeof(path));
		_splitpath(path, NULL, mxepath, NULL, NULL);
		_snprintf(dllpath, MAX_PATH, "%s%s", mxepath, name);
		// now try to load the dll:
		hm = LoadLibrary(dllpath);
		if (hm == NULL) {
			error("failed to load %s at %s", name, dllpath);
			post("please make sure that %s is located next to leap.mxe", name);
			return 0;
		}
	}
#endif
	
	common_symbols_init();
    ps_frame_start = gensym("frame_start");
    ps_frame_end = gensym("frame_end");
	ps_frame = gensym("frame");
	ps_hand = gensym("hand");
	ps_finger = gensym("finger");
	ps_palm = gensym("palm");
	ps_ball = gensym("ball");
	ps_fps = gensym("fps");
	ps_connected = gensym("connected");
	ps_probability = gensym("probability");
	
    maxclass = class_new("leap", (method)leap_new, (method)leap_free, (long)sizeof(t_leap),
                         0L, A_GIMME, 0);
    
    class_addmethod(maxclass, (method)leap_assist, "assist", A_CANT, 0);
	// class_addmethod(maxclass, (method)leap_notify, "notify", A_CANT, 0);
    
    class_addmethod(maxclass, (method)leap_jit_matrix, "jit_matrix", A_SYM, 0);
    class_addmethod(maxclass, (method)leap_bang, "bang", 0);
	class_addmethod(maxclass, (method)leap_configure, "configure", 0);
    
	//CLASS_ATTR_FLOAT(maxclass, "predict", 0, t_leap, predict);
	//
	
	CLASS_ATTR_LONG(maxclass, "unique", 0, t_leap, unique);
	CLASS_ATTR_STYLE_LABEL(maxclass, "unique", 0, "onoff", "unique: output only new frames");
	
	CLASS_ATTR_LONG(maxclass, "allframes", 0, t_leap, allframes);
	CLASS_ATTR_STYLE_LABEL(maxclass, "allframes", 0, "onoff", "allframes: output all frames between each bang");
	
	CLASS_ATTR_LONG(maxclass, "images", 0, t_leap, images);
	CLASS_ATTR_STYLE_LABEL(maxclass, "images", 0, "onoff", "images: output raw IR images from the sensor");
	CLASS_ATTR_ACCESSORS(maxclass, "images", NULL, leap_images_set);
	
	CLASS_ATTR_LONG(maxclass, "hmd", 0, t_leap, hmd);
	CLASS_ATTR_STYLE_LABEL(maxclass, "hmd", 0, "onoff", "hmd: enable to optimize for head-mounted display (LeapVR)");
	CLASS_ATTR_ACCESSORS(maxclass, "hmd", NULL, leap_hmd_set);
	
	CLASS_ATTR_LONG(maxclass, "background", 0, t_leap, background);
	CLASS_ATTR_STYLE_LABEL(maxclass, "background", 0, "onoff", "background: enable data capture when app has lost focus");
	CLASS_ATTR_ACCESSORS(maxclass, "background", NULL, leap_hmd_set);
	
	CLASS_ATTR_LONG(maxclass, "motion_tracking", 0, t_leap, motion_tracking);
	CLASS_ATTR_STYLE_LABEL(maxclass, "motion_tracking", 0, "onoff", "motion_tracking: output estimated rotation/scale/translation between polls");
	
	CLASS_ATTR_LONG(maxclass, "serialize", 0, t_leap, serialize);
	CLASS_ATTR_STYLE_LABEL(maxclass, "serialize", 0, "onoff", "serialize: output serialized frames");
	
	CLASS_ATTR_LONG(maxclass, "aka", 0, t_leap, aka);
	CLASS_ATTR_STYLE_LABEL(maxclass, "aka", 0, "onoff", "aka: provide output compatible with aka.leapmotion");
	
    class_register(CLASS_BOX, maxclass); 
    leap_class = maxclass;
    return 0;
}
