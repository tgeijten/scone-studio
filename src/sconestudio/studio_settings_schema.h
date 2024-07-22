const char* studio_settings_schema = R"str(
file {
	label = "File"
	auto_write_scone_evaluation { type = bool label = "Automatically write data after evaluating a .scone file" default = 0 }
	auto_write_par_evaluation { type = bool label = "Automatically write data after evaluating a .par file" default = 1 }
	playback_start { type = float label = "Time to start playback, negative numbers start from end" default = 0 }
}

gait_analysis {
	label = "Gait Analysis"
	template { type = path default = "" label = "Template used for gait analysis" }
	plot_individual_cycles { type = bool default = 1 label = "Plot individual gait cycles" }
	show_fit { type = bool default = 1 label = "Show data fit" }
	force_threshold { type = float default = 0.001 label = "Force threshold [BW] for step detection" }
	min_stance_duration { type = float default = 0.1 label = "Minimum contact length [s] for stance detection" }
	skip_first { type = int default = 2 label = "Number of initial steps to exclude from analysis" }
	skip_last { type = int default = 1 label = "Number of final steps to exclude from analysis" }
	title_font_size { type = int default = 11 label = "Gait plot title font point size" }
	axis_font_size { type = int default = 10 label = "Gait plot axis font point size" }
}

video {
	label = "Video"
	path_to_ffmpeg { type = path default = "" label = "Location of ffmpeg.exe (required for videos)" }
	frame_rate { type = float label = "Video output frame rate" default = 30 }
	quality { type = int default = 2 label = "Quality for video output" }
	width { type = int default = 1024 label = "Horizontal resolution (only when Viewer is not docked)" }
	height { type = int default = 768 label = "Vertical resolution (only when Viewer is not docked)" }
}

viewer {
	label = "3D Viewer"
	background { type = color label = "Background color" default = 0xc0c0c0 }
	tile1 { type = color label = "Viewer Tile A color" default = 0xc4c4c4 }
	tile2 { type = color label = "Viewer Tile B color" default = 0xbcbcbc }
	tile_size { type = float label = "Viewer tile size" default = 0.5 }
	tile_count_x { type = int label = "Viewer tile count in x direction" default = 128 }
	tile_count_z { type = int label = "Viewer tile count in z direction" default = 128 }
	tiles_follow_body { type = bool label = "Automatically adjust ground tiles to always remain visible" default = 1 }
	max_shadow_dist { type = real label = "Maximum distance from the focus point at which shadows are renderd" default = 10 }
	bone { type = color label = "Bone color" default { h = 40 s = 0.2 v = 0.8 } }
	tendon { type = color label = "Tendon color" default { h = 40 s = 0.02 v = 0.8 } }
	ligament { type = color label = "Tendon color" default { h = 40 s = 0.4 v = 0.8 } }
	force { type = color label = "Force arrow color" default { h = 60 s = 0.8 v = 0.8 } }
	force_alpha { type = float label = "Force arrow transparency" default = 1 }
	forces_cast_shadows { type = bool label = "Force arrows cast shadows" default = 0 }
	combine_contact_forces { type = bool label = "Combine body contact forces into a single arrow" default = 0 }
	joint_forces_are_for_parents { type = bool label = "Show joint forces and torques in the direction of parent bodies" default = 0 }
	moment { type = color label = "Moment color" default { h = 5 s = 0.8 v = 0.8 } }
	contact { type = color label = "Contact color" default { h = 40 s = 0.2 v = 0.8 } }
	contact_alpha { type = float label = "Contact color transparency" default = 0.2 }
	contact_geometries_cast_shadows { type = bool label = "Contact geometries cast shadows" default = 0 }
	auxiliary { type = color label = "Auxiliary color" default { h = 180 s = 0.5 v = 0.8 } }
	auxiliary_alpha { type = float label = "Auxiliary color transparency" default = 0.5 }
	static { type = color label = "Static geometry color" default { h = 180 s = 0.25 v = 0.4 } }
	object { type = color label = "Object geometry color" default { h = 180 s = 0.02 v = 0.55 } }
	joint { type = color label = "Joint color" default { h = 40 s = 0.2 v = 0.5 } }
	com { type = color label = "Center of Mass color" default { h = 105 s = 0.5 v = 0.5 } }
	muscle_min100 { type = color label = "Muscle color at -100%" default { h = 120 s = 0.7 v = 0.65 } }
	muscle_0 { type = color label = "Muscle color at 0%" default { h = 240 s = 0.7 v = 0.65 } }
	muscle_50 { type = color label = "Muscle color at 50%" default { h = 0 s = 0.8 v = 0.8 } }
	muscle_100 { type = color label = "Muscle color at 100%" default { h = 0 s = 0.9 v = 1.0 } }
	specular { type = float label = "Model specular" default = 1 }
	shininess { type = float label = "Model shininess" default = 30 }
	ambient { type = float label = "Model ambient" default = 1 }
	ambient_intensity { type = color label = "Ambient intensity in scene" default = 0.7 }
	auto_muscle_width { type = bool label = "Muscle diameter represents PCSA" default = true }
	auto_muscle_width_factor { type = float label = "Muscle diameter scaling factor" default = 0.25 }
	muscle_width { type = float label = "Muscle line radius (if not derived from PCSA)" default = 0.005 }
	muscle_position { type = float label = "Relative muscle belly position (0-1)" range = [ 0 1 ] default = 0.333 }
	relative_tendon_width { type = float label = "Tendon diameter relative to muscle" range = [ 0.1 1 ] default = 0.618 }
	joint_radius { type = float label = "Joint sphere radius" range = [ 0.001 1 ] default = 0.015 }
	enable_object_cache { type = bool label = "Enable mesh caching for faster reloading (needs restart)" default = 1 }
	hud_type { type = int label = "Show logo" default = 1 }
	camera_follow_body { type = string label = "Body to follow by camera (leave blank for CoM)" default = "" }
	camera_light_offset { type = vec3 label = "Camera light position offset" default = "-2 8 3" }
	camera_orbit_speed { type = float label = "Medium camera orbit speed" range = [ 0 360 ] default = 10 }
	camera_transition_duration { type = float label = "Duration of camera transitions during playback" range = [ 0 60 ] default = 0.5 }
}

optimization {
	label = "Optimization"
	use_external_process { type = bool default = 0 label = "Perform optimizations using external process" }
}

progress {
	label = "Optimization progress"
	update_interval { type = int default = 250 label = "Interval [ms] to update progress graphs" range = [ 10 1000 ] }
	line_width { type = float default = 1 label = "Line width of progress graphs (use 1 for best performance)" range = [ 1 10 ] }
	show_prediction { type = bool default = 0 label = "Show predicted fitness" }
	show_fitness_label { type = bool default = 0 label = "Show fitness label on progress graph" }
	max_rows_below_results { type = int default = 3 label = "Maximum number of graph rows below results" }
	max_columns_below_results { type = int default = 2 label = "Maximum number of graph columns below results" }
	max_rows_left_of_results { type = int default = 6 label = "Maximum number of graph rows left of results" }
}

analysis {
	label = "Analysis"
	line_width { type = float default = 1 label = "Line width for analysis (use 1 for best performance)" range = [ 1 10 ] }
	auto_fit_vertical_axis { type = bool default = 1 label = "Automatically scale vertical axis to fit data in range" }
}

muscle_analysis {
	label = "Muscle Analysis"
	step_size { type = float default = 1 label = "Increments to use in Muscle Analysis graph [deg]" range = [ 0.01 10 ] }
	max_steps { type = int default = 360 label = "Maximum number of samples in Muscle Analysis graph" range = [ 1 10000 ] }
	analyze_on_load { type = bool default = 0 label = "Perform muscle analysis on scenario load" }
}

coordinates {
	label = "Coordinates"
	export_activations { type = bool label = "Export activations" default = 0 }
}

sconegym {
	label = "Sconegym"
	python { type = string label = "Name of the Python executable used for evaluation" default = "python" }
	num_episodes { type = int label = "Number of episodes to evaluate" default = 5 }
}

ui {
	label = "User Interface"
	log_level { type = int default = 2 label = "Messages log level (1-7)" }
	enable_profiler { type = bool default = 0 label = "Enable GUI profiler" }
	show_conversion_support_message { type = bool default = 1 label = "Show support message after convert to Hyfydy" }
	reset_layout { type = bool default = 0 label = "Reset window layout on start" }
	last_version_run { type = string default = "" label = "Last version run" }
	show_startup_time { type = bool default = 0 label = "Show startup time" }
	check_tutorials_new_version { type = bool default = 1 label = "Check if Tutorials and Examples are up-to-date on new version" }
	check_tutorials_on_launch { type = bool default = 1 label = "Check if Tutorials and Examples are up-to-date on launch" }
	tutorials_version { type = int default = 3 label = "Version of the Tutorials and Examples to install" }
}
)str";