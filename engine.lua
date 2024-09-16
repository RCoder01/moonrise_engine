Debug = {
    Log = function(message)
    end,

    LogError = function(message)
    end
}

Application = {
    Quit = function()
    end,

    Sleep = function(ms)
    end,

    GetFrame = function()
        return 0
    end,

    OpenUrl = function(url)
    end,

    GetTime = function()
        return 0
    end,
}

local actor_type = {
    GetName = function(actor)
        return ""
    end,

    GetID = function(actor)
        return 0
    end,

    GetComponent = function(actor, component_type)
        return nil
    end,

    GetComponents = function(actor, component_type)
        return {}
    end,

    GetComponentByKey = function(actor, component_key)
        return nil
    end,

    AddComponent = function(actor, component_name)
    end,

    RemoveComponent = function(actor, component_ref)
    end
}

vec2 = {
    x = 0,
    y = 0,

    Add = function(v0, v1)
        return vec2
    end,

    Sub = function(v0, v1)
        return vec2
    end,

    Mul = function(v, c)
        return vec2
    end,

    Div = function(v, c)
        return vec2
    end,

    Magnitude = function(vec)
        return 0
    end,

    Normalize = function(vec)
    end,

    Dot = function(v1, v2)
        return 0
    end
}

setmetatable(vec2, {
    __call = function(x, y)
        return vec2
    end,
})

vec3 = {
    x = 0,
    y = 0,
    z = 0,
    yaw = 0, -- alias for x
    pitch = 0, -- alias for y
    roll = 0, -- alias for z
    
    Add = function(v0, v1)
        return vec3
    end,

    Sub = function(v0, v1)
        return vec3
    end,

    Mul = function(vec, scalar)
        return vec3
    end,

    Div = function(vec, scalar)
        return vec3
    end,

    Magnitude = function(vec)
        return 0
    end,

    Normalize = function(vec)
    end,

    Dot = function(v1, v2)
        return 0
    end,

    Cross = function(v1, v2)
        return 0
    end
}

Transform = {
    Identity = function()
        return Transform
    end,

    translation = vec3,
    translation_x = 0, -- alias for translation.x, will update the transform if modified
    translation_y = 0, -- alias for translation.y, will update the transform if modified
    translation_z = 0, -- alias for translation.z, will update the transform if modified
    rotation = vec3,
    rotation_yaw = 0, -- alias for rotation.yaw, will update the transform if modified
    rotation_pitch = 0, -- alias for rotation.pitch, will update the transform if modified
    rotation_roll = 0, -- alias for rotation.roll, will update the transform if modified
    scale = vec3,
    scale_x = 0, -- alias for scale.x, will update the transform if modified
    scale_y = 0, -- alias for scale.y, will update the transform if modified
    scale_z = 0, -- alias for scale.z, will update the transform if modified

    ToString = function (transform)
        return ""
    end,

    Add = function (t0, t1)
        return Transform
    end,

    Mul = function (transform, scalar)
        return Transform
    end
}

Model = {
    key = "",
    actor = actor_type,
    enabled = false,
    type = "",
    mesh = "",
    transform = Transform,
    translation = vec3, -- alias for transform.translation, will update the transform if modified
    translation_x = 0, -- alias for translation.translation.x, will update the transform if modified
    translation_y = 0, -- alias for translation.translation.y, will update the transform if modified
    translation_z = 0, -- alias for translation.translation.z, will update the transform if modified
    rotation = vec3, -- alias for transform.rotation, will update the transform if modified
    rotation_yaw = 0, -- alias for translation.rotation.yaw, will update the transform if modified
    rotation_pitch = 0, -- alias for translation.rotation.pitch, will update the transform if modified
    rotation_roll = 0, -- alias for translation.rotation.roll, will update the transform if modified
    scale = vec3, -- alias for transform.scale, will update the transform if modified
    scale_x = 0, -- alias for translation.scale.x, will update the transform if modified
    scale_y = 0, -- alias for translation.scale.y, will update the transform if modified
    scale_z = 0, -- alias for translation.scale.z, will update the transform if modified

    OnStart = function(model)
    end,

    OnUpdate = function(model)
    end,

    OnDestroy = function(model)
    end,
}

Actor = {
    Find = function(name)
        return actor_type
    end,

    FindAll = function(name)
        return {}
    end,

    Instantiate = function(prefab)
        return actor_type
    end,

    Destroy = function(actor)
    end
}

Input = {
    GetKey = function(key)
        return false
    end,

    GetKeyDown = function(key)
        return false
    end,

    GetKeyUp = function(key)
        return false
    end,

    GetMousePosition = function()
        return {x = 0, y = 0}
    end,

    GetMouseButton = function(button)
        return false
    end,

    GetMouseButtonDown = function(button)
        return false
    end,

    GetMouseButtonUp = function(button)
        return false
    end,

    GetMouseScrollDelta = function()
        return {x = 0, y = 0}
    end
}

Audio = {
    Play = function(channel, sound, loop)
    end,

    Halt = function(channel)
    end,

    SetVolume = function(channel, volume)
    end
}

Scene = {
    Load = function(scene)
    end,

    GetCurrent = function()
        return ""
    end,

    DontDestroy = function(actor)
    end
}

Event = {
    Publish = function(event, ...)
    end,

    Subscribe = function(event, component, callback)
    end,

    Unsubscribe = function(event, component, callback)
    end
}

Math = {
    Random = function(min, max)
        return 0
    end,

    Rotate = function(vec3, yawpitchroll) -- yawpitchroll is a vec3
        return vec3
    end,

    RotationCompose = function(a, b)
        return vec3
    end,
}

Camera = {
    transform = Transform.identity()
}
