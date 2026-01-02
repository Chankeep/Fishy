#include "Camera.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>


namespace Fishy {

void Camera::update(float mouseDeltaX, float mouseDeltaY, float scrollDelta, bool mouseButtonRight,
					bool mouseButtonMiddle, bool mouseButtonLeft, float deltaTime) {
	// Rotate only with right mouse button
	if (mouseButtonRight) {
		rotate(mouseDeltaX, mouseDeltaY);
	}

	// Pan with middle mouse button
	if (mouseButtonMiddle) {
		pan(mouseDeltaX, mouseDeltaY);
	}

	// Zoom with mouse wheel
	if (std::abs(scrollDelta) > 0.0f) {
		zoom(scrollDelta);
	}
}

void Camera::rotate(float deltaX, float deltaY) {
	_yaw -= deltaX * _rotateSpeed;
	_pitch += deltaY * _rotateSpeed; // Fixed: now moves in correct direction

	// Clamp pitch to prevent gimbal lock
	_pitch = std::clamp(_pitch, _minPitch, _maxPitch);

	updatePosition();
}

void Camera::zoom(float delta) {
	_distance -= delta * _zoomSpeed * _distance;

	// Clamp distance
	_distance = std::clamp(_distance, _minDistance, _maxDistance);

	updatePosition();
}

void Camera::pan(float deltaX, float deltaY) {
	// Get camera right and up vectors
	glm::vec3 right = getRight();
	glm::vec3 up = getUp();

	// Pan in world space (aligned with camera view)
	// Note: We pan the target, not the camera position directly
	float panX = -deltaX * _panSpeed * _distance;
	float panY = deltaY * _panSpeed * _distance;

	_target += right * panX;
	_target += up * panY;

	updatePosition();
}

void Camera::updatePosition() {
	// Convert spherical coordinates to Cartesian position
	// Y-up coordinate system

	float yawRad = glm::radians(_yaw);
	float pitchRad = glm::radians(_pitch);

	// Spherical to Cartesian conversion
	float x = _distance * glm::cos(pitchRad) * glm::sin(yawRad);
	float y = _distance * glm::sin(pitchRad);
	float z = _distance * glm::cos(pitchRad) * glm::cos(yawRad);

	_position = _target + glm::vec3(x, y, z);
}

glm::mat4 Camera::getViewMatrix() const {
	// Use lookAt to create view matrix
	// eye: camera position, center: target, up: world up (Y-up)
	return glm::lookAt(_position, _target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::getProjectionMatrix(float aspect, float fov, float nearPlane, float farPlane) const {
	glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
	proj[1][1] *= -1; // Invert Y for Vulkan
	return proj;
}

glm::vec3 Camera::getForward() const { return glm::normalize(_target - _position); }

glm::vec3 Camera::getUp() const {
	glm::vec3 forward = getForward();
	glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

	// Calculate right vector first
	glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));

	// Up is perpendicular to forward and right
	return glm::cross(right, forward);
}

glm::vec3 Camera::getRight() const {
	glm::vec3 forward = getForward();
	glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	return glm::normalize(glm::cross(forward, worldUp));
}

void Camera::setDistance(float distance) {
	_distance = std::clamp(distance, _minDistance, _maxDistance);
	updatePosition();
}

void Camera::reset() {
	_target = glm::vec3(0.0f);
	_distance = 3.0f;
	_yaw = 0.0f;
	_pitch = 0.0f;
	updatePosition();
}

} // namespace Fishy
